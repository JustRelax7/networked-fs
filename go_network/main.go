package main

/*
#cgo LDFLAGS: -L../build -lengine
#include <stdlib.h>

int fs_init(const char* disk_path);
int fs_write(const char* path, const char* data, int size);
char* fs_read(const char* path, int* out_size);
char* fs_list();
int delete_file(const char* path);
void fs_unmount();
*/
import "C"
import (
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"os/signal"
	"slices"
	"strings"
	"sync"
	"syscall"
	"time"
	"unsafe"

	"github.com/gin-gonic/gin"
)

type writeJobData struct{
    filename string
    filedata [] byte
}

type readResult struct{
    filedata [] byte
    err bool
}

type readJob struct{
    filename string
    result chan readResult
}

type writeJob struct{
    writedata writeJobData
    result chan bool
}

type deleteJob struct{
    filename string
    result chan int
}

const readQueueSize=10
const writeQueueSize=3
const deleteQueueSize=3

var(
    //Mutexes
    fileLocks map[string] *sync.RWMutex
    fileListMutex *sync.RWMutex
    fileLocksMutex *sync.Mutex

    //Filelist
    fileList []string
    // Job Queues
    readQueue chan readJob
    writeQueue chan writeJob
    deleteQueue chan deleteJob
)

func InitDisk()int{
    temp := C.CString("../cpp_engine/virtual_disk.bin")
    C.fs_init(temp)
    defer C.free(unsafe.Pointer(temp))
    fileListPointer := C.fs_list()   
    fileListMutex = &sync.RWMutex{}
    fileLocksMutex = &sync.Mutex{}
    fileLocks = make(map[string] *sync.RWMutex)
    if fileListPointer!=nil{
        s := C.GoString(fileListPointer)
        fileList = strings.Split(s,"\n")
        C.free(unsafe.Pointer(fileListPointer)) 
    }else{
        return -1;
    }
    fmt.Println("[Go Server] File List received : ", fileList)
    return 0;
}

func readworker(jobs <- chan readJob){
    for j := range jobs{
        name := j.filename
        fileLocksMutex.Lock()
        if fileLocks[name] == nil{
            fileLocks[name] = &sync.RWMutex{}
        }
        mu := fileLocks[name]
        fileLocksMutex.Unlock()
        temp_name := C.CString(name)
        var size C.int
        mu.RLock()
        buffer := C.fs_read(temp_name,&size)
        mu.RUnlock()
        C.free(unsafe.Pointer(temp_name))   
        if buffer == nil{
            j.result <- readResult{
                filedata : nil,
                err: true,
            }
        }else{
            file_data := C.GoBytes(unsafe.Pointer(buffer), size)
            C.free(unsafe.Pointer(buffer))
            j.result <- readResult{
                filedata : file_data,
                err: false,
            }
        }
    }
}

func getFileHandler(c *gin.Context) {
    // 1. Create a "one-time" channel for this specific request
    // Buffered by 1 so the worker doesn't block when sending
    fileName := c.Param("filename")
    flag := false
    fileListMutex.RLock()
    for _,foile := range fileList{
        if foile == fileName{
            flag=true
            break
        }
    }
    fileListMutex.RUnlock()
    if flag==false{
        c.JSON(http.StatusNotFound,gin.H{"error" : "File does not exist"})
        return
    }
    myResultChan := make(chan readResult, 1)

    // 2. Push to the Worker Pool
    select {
        case readQueue <- readJob{
            filename: fileName,
            result:   myResultChan,
        }:
        default:
            c.JSON(http.StatusServiceUnavailable, gin.H{"error": "Server busy"})
            return
    }
    // 3. WAIT for the result (This is efficient in Go)
    // Use a select to add a timeout so the user isn't stuck forever
    select {
    case result := <-myResultChan:
        if result.err == true {
            c.JSON(http.StatusInternalServerError, gin.H{"error": "File read failed"})
            return
        }
        // Success! Send to JS
        c.Header("Content-Disposition", "attachment; filename="+c.Param("filename"))
        c.Data(http.StatusOK, "application/octet-stream", result.filedata)

    case <-time.After(5 * time.Second): // Timeout
        c.JSON(http.StatusGatewayTimeout, gin.H{"error": "Read timeout"})
    }
}

func writeworker(jobs <- chan writeJob){
    for j := range jobs{
        // File 
        tempFoile := j.writedata.filename
        // FileList Updating
        flager := false
        fileListMutex.RLock()
        for _,foile := range fileList{
            if foile == tempFoile{
                flager=true
                break
            }
        }
        fileListMutex.RUnlock()
        if flager == false{
            fileListMutex.Lock()
            fileList = append(fileList, tempFoile)
            fileListMutex.Unlock()
        }else{
            flager=false
            old_ver:=tempFoile+".v_old"
            fileListMutex.RLock()
            for _,foile := range fileList{
                if foile == old_ver{
                    flager=true
                    break
                }
            }
            fileListMutex.RUnlock()
            if flager == false{
                fileListMutex.Lock()
                fileList = append(fileList, old_ver)
                fileListMutex.Unlock()
            }
        } 
        fileLocksMutex.Lock()
        if fileLocks[tempFoile] == nil{
            fileLocks[tempFoile] = &sync.RWMutex{}
        }
        mu := fileLocks[tempFoile]
        fileLocksMutex.Unlock()
        data :=  j.writedata.filedata
        fileName := C.CString(tempFoile)
        fileData :=  (*C.char)(unsafe.Pointer(&data[0]))
        mu.Lock()
        val := C.fs_write(fileName,fileData,C.int(len(data)))
        mu.Unlock()
        C.free(unsafe.Pointer(fileName))
        if val == -1{
            j.result <- false
        }else{
            j.result <- true
        }
    }
}

func fileUploadHandler(c *gin.Context){
    file,err := c.FormFile("file")
    if err != nil{
        fmt.Println("err occured")
        return
    }
    fmt.Println("Data received : ", file.Filename)
    f,err := file.Open()
    if err != nil {
        fmt.Println("Error opening file:", err)
        return
    }
    defer f.Close()
    data, _ := io.ReadAll(f)
    fmt.Println("File content received successfully")
    flag := make(chan bool,1)
    select {
        case writeQueue <- writeJob{
            writedata : writeJobData{
                filename: file.Filename,
                filedata: data,
            },
            result: flag,
        }:
        default:
            c.JSON(http.StatusServiceUnavailable, gin.H{"error": "Server busy"})
            return
    }

    result := <-flag
    if result == false {
        c.JSON(http.StatusInternalServerError, gin.H{"error": "File write failed"})
        return
    }
    // Success! Send to JS
    c.JSON(http.StatusOK, gin.H{"Message" : "File write saved to Disk succesfully."})
    
}


func deleteworker(jobs <- chan deleteJob){
    for j := range jobs{
        flager := false
        ind := 0
        fileName := j.filename
        fileListMutex.RLock()
        for i,foile := range fileList{
            if foile == fileName{
                flager=true
                ind = i
                break
            }
        }
        fileListMutex.RUnlock()
        if flager==false{
            j.result <- -1
            return
        }   
        cfile := C.CString(fileName)
        val := C.delete_file(cfile)
        C.free(unsafe.Pointer(cfile))
        fileListMutex.Lock()
        fileList = slices.Delete(fileList,ind,ind+1)
        fileListMutex.Unlock()
        if val==-1{
            j.result <- -2
            return
        }

        j.result <- 0
    }
}

func deleteFileHandler(c *gin.Context){
    fileName := c.Param("filename")
    flag := make(chan int,1)
    deleteQueue <- deleteJob{
        filename: fileName,
        result: flag,
    }

    result := <-flag
    if result == -1 {
        c.JSON(http.StatusNotFound, gin.H{"error": "File not found"})
        return
    }else if result == -2{
        c.JSON(http.StatusInternalServerError, gin.H{"error": "File could not be deleted"})
        return
    }
    // Success! Send to JS
    c.JSON(http.StatusOK, gin.H{"Message" : "File deleted succesfully."})
}

func main() {
	// router.GET("/albums/:id", getAlbumbyID)

    //Initialise Disk
    val := InitDisk()

    // Ensure it unmounts cleanly when the server closes
	defer C.fs_unmount()

	// Catch Ctrl+C so the defer actually runs before the program dies
	c := make(chan os.Signal, 1)
	signal.Notify(c, os.Interrupt, syscall.SIGTERM)
    go func() {
		<-c // Wait for the interrupt signal
		fmt.Println("\nClosing all the worker Queues")
        close(readQueue)
        close(writeQueue)
        close(deleteQueue)
		fmt.Println("\n[Server] Shutting down... Unmounting disk cleanly.")
		C.fs_unmount() // Safely destroy C++ objects in RAM
		os.Exit(0)
	}()

    if val==-1{
        log.Fatal("InitDisk failed")
        return;
    }
    readQueue = make(chan readJob,readQueueSize)
    writeQueue = make(chan writeJob,writeQueueSize)
    deleteQueue = make(chan deleteJob,deleteQueueSize)

    //Initialise read workers.
    for i:=0 ; i<readQueueSize ; i++{
        go readworker(readQueue)
    }

    //Initialise write workers.
    for i:=0 ; i<writeQueueSize ; i++{
        go writeworker(writeQueue)
    }
    
    // Initialise write workers.
    for i:=0 ; i<deleteQueueSize ; i++{
        go deleteworker(deleteQueue)
    }

    router := gin.Default()
    //  Limit multipart forms to 8MB
    router.MaxMultipartMemory = 8 << 20
    router.GET("/",func(c *gin.Context) {
        c.JSON(http.StatusOK,fileList)
    })
    router.GET("/:filename", getFileHandler)
	router.POST("/files/", fileUploadHandler)
    router.DELETE("/:filename",deleteFileHandler)
    log.Println("Server running on localhost:8080")
    log.Fatal(router.Run("10.196.25.145:8080"))
}