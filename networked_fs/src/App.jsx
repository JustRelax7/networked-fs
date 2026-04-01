import React, { useState, useEffect, useRef } from 'react';
import axios from 'axios';
import toast, { Toaster } from 'react-hot-toast';
import { Upload, Download, Trash2, HardDrive, ShieldCheck, Cpu, Clock, File as FileIcon} from 'lucide-react';
import { BACKEND_URL } from './config';

export default function App() {
  const [view, setView] = useState('landing'); // 'landing' or 'dashboard'
  const [allFiles, setAllFiles] = useState([]);
  const [activeUploads, setActiveUploads] = useState({});
  const [activeDownloads, setActiveDownloads] = useState({});
  const fileInputRef = useRef(null);

  // Fetch file list from Go server
  const fetchFiles = async () => {
    try {
      const res = await axios.get(`${BACKEND_URL}/files`);
      console.log(res.data)
      // Go returns null for empty slice, handle it by defaulting to []
      setAllFiles(res.data || []);
    } catch (err) {
      toast.error("Failed to connect to backend engine.");
    }
  };

  useEffect(() => {
    if (view === 'dashboard') {
      fetchFiles();
    }
  }, [view]);

  // Derived state: Filter out .v_old files and reverse for chronological order
  const displayFiles = allFiles
    .filter(f => !f.endsWith('.v_old'))
    .reverse();

  // --- HANDLERS ---

  const handleUploadClick = () => fileInputRef.current.click();

  const handleFileUpload = async (e) => {
    const file = e.target.files[0];
    if (!file) return;

    const formData = new FormData();
    formData.append('file', file);

    const uploadId = Date.now().toString();
    setActiveUploads(prev => ({ ...prev, [uploadId]: { name: file.name, progress: 0 } }));

    try {
      const res = await axios.post(`${BACKEND_URL}/upload`, formData, {
        headers: { 'Content-Type': 'multipart/form-data' },
        onUploadProgress: (progressEvent) => {
          const percentCompleted = Math.round((progressEvent.loaded * 100) / progressEvent.total);
          setActiveUploads(prev => ({
            ...prev,
            [uploadId]: { ...prev[uploadId], progress: percentCompleted }
          }));
        }
      });
      toast.success(res.data.Message || "Upload successful!");
      fetchFiles();
    } catch (err) {
      toast.error(err.response?.data?.error || "Upload failed");
    } finally {
      setActiveUploads(prev => {
        const newUploads = { ...prev };
        delete newUploads[uploadId];
        return newUploads;
      });
      e.target.value = null; // reset input
    }
  };

  const handleDownload = async (filename) => {
    const downloadId = Date.now().toString();
    setActiveDownloads(prev => ({ ...prev, [downloadId]: { name: filename, progress: 0 } }));

    try {
      const res = await axios.get(`${BACKEND_URL}/files/${filename}`, {
        responseType: 'blob', // Crucial for receiving byte slices
        onDownloadProgress: (progressEvent) => {
          // Note: total might be undefined depending on Go's Content-Length headers
          const percentCompleted = progressEvent.total ? Math.round((progressEvent.loaded * 100) / progressEvent.total) : 50; 
          setActiveDownloads(prev => ({
            ...prev,
            [downloadId]: { ...prev[downloadId], progress: percentCompleted }
          }));
        }
      });

      // Create a temporary link to trigger the browser download
      const url = window.URL.createObjectURL(new Blob([res.data]));
      const link = document.createElement('a');
      link.href = url;
      link.setAttribute('download', filename);
      document.body.appendChild(link);
      link.click();
      link.remove();
      toast.success(`${filename} downloaded!`);
    } catch (err) {
      toast.error(`Failed to download ${filename}`);
    } finally {
      setActiveDownloads(prev => {
        const newDl = { ...prev };
        delete newDl[downloadId];
        return newDl;
      });
    }
  };

  const handleDelete = async (filename, type) => {
    try {
      if (type === 'current' || type === 'both') {
        await axios.delete(`${BACKEND_URL}/files/${filename}`);
      }
      if (type === 'old' || type === 'both') {
        await axios.delete(`${BACKEND_URL}/files/${filename}.v_old`);
      }
      toast.success(`Deleted successfully!`);
      fetchFiles();
    } catch (err) {
      toast.error(err.response?.data?.error || "Deletion failed");
    }
  };

  // --- UI COMPONENTS ---

  const ProgressBar = ({ item }) => (
    <div className="mb-2">
      <div className="flex justify-between text-xs mb-1 text-gray-600">
        <span className="truncate w-32">{item.name}</span>
        <span>{item.progress}%</span>
      </div>
      <div className="w-full bg-gray-200 rounded-full h-1.5">
        <div className="bg-blue-600 h-1.5 rounded-full" style={{ width: `${item.progress}%` }}></div>
      </div>
    </div>
  );

  return (
    <div className="min-h-screen bg-gray-50 font-sans text-gray-800 selection:bg-blue-200">
      <Toaster position="top-right" />

      {/* Navigation */}
      <nav className="bg-white border-b border-gray-200 sticky top-0 z-50">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 h-16 flex items-center justify-between">
          <div className="flex items-center gap-2 cursor-pointer" onClick={() => setView('landing')}>
            <HardDrive className="text-blue-600" />
            <span className="font-bold text-xl tracking-tight">Networked FS</span>
          </div>
          {view === 'landing' && (
            <button 
              onClick={() => setView('dashboard')}
              className="bg-blue-600 text-white px-5 py-2 rounded-md font-medium hover:bg-blue-700 transition-colors"
            >
              Open Disk
            </button>
          )}
        </div>
      </nav>

      {view === 'landing' ? (
        // --- LANDING PAGE ---
        <div className="animate-in fade-in duration-500">
          {/* Hero */}
          <section className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-24 text-center">
            <h1 className="text-5xl font-extrabold tracking-tight text-gray-900 mb-6">
              A High-Performance <span className="text-blue-600">Virtual File System</span>
            </h1>
            <p className="text-xl text-gray-600 max-w-2xl mx-auto mb-10">
              Built from scratch in C++ and Go. Featuring custom memory bitmaps, Copy-on-Write versioning, and highly concurrent worker pools.
            </p>
            <button 
              onClick={() => setView('dashboard')}
              className="bg-blue-600 text-white px-8 py-3 rounded-lg text-lg font-semibold hover:bg-blue-700 shadow-lg hover:shadow-xl transition-all"
            >
              Access Dashboard
            </button>
          </section>

          {/* Novelties / Features */}
          <section className="bg-white py-20 border-y border-gray-200">
            <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
              <h2 className="text-3xl font-bold text-center mb-12">System Architecture Novelties</h2>
              <div className="grid md:grid-cols-3 gap-8">
                <div className="p-6 bg-gray-50 rounded-xl border border-gray-100">
                  <ShieldCheck className="w-10 h-10 text-blue-600 mb-4" />
                  <h3 className="text-xl font-bold mb-2">C++ Engine & Versioning</h3>
                  <p className="text-gray-600 text-sm">
                    A raw virtual disk block device handling Inodes and memory bitmaps. Includes structural Copy-on-Write (CoW) versioning to preserve file history securely.
                  </p>
                </div>
                <div className="p-6 bg-gray-50 rounded-xl border border-gray-100">
                  <Cpu className="w-10 h-10 text-blue-600 mb-4" />
                  <h3 className="text-xl font-bold mb-2">Go Worker Pools</h3>
                  <p className="text-gray-600 text-sm">
                    Instead of flooding the disk, the Go backend routes requests through strict concurrent channels (Read/Write/Delete Queues) to prevent RAM bloating.
                  </p>
                </div>
                <div className="p-6 bg-gray-50 rounded-xl border border-gray-100">
                  <Clock className="w-10 h-10 text-blue-600 mb-4" />
                  <h3 className="text-xl font-bold mb-2">Rock-Solid Synchronization</h3>
                  <p className="text-gray-600 text-sm">
                    Utilizes sync.RWMutex per individual file, alongside global mutexes for the file list and lock maps, ensuring zero race conditions during heavy I/O.
                  </p>
                </div>
              </div>
            </div>
          </section>

          {/* Guide */}
          <section className="max-w-4xl mx-auto px-4 py-20">
            <h2 className="text-3xl font-bold text-center mb-8">How to Use</h2>
            <div className="prose prose-blue mx-auto text-gray-600">
              <ul className="space-y-4">
                <li><strong>Uploading:</strong> Click the "Upload File" button in the dashboard. The system allocates Inodes and physical blocks on the virtual disk automatically.</li>
                <li><strong>Versioning:</strong> If you upload a file that already exists, the old file is safely renamed to <code>.v_old</code> behind the scenes via CoW.</li>
                <li><strong>Downloading:</strong> You can fetch the current live version, or click "Download Old" to retrieve the previous state of the file.</li>
                <li><strong>Deleting:</strong> Free up virtual disk space by deleting the current version, the old version, or purging both simultaneously to return blocks to the Bitmap Manager.</li>
              </ul>
            </div>
          </section>
        </div>
      ) : (
        // --- DASHBOARD PAGE ---
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8 animate-in slide-in-from-bottom-4 duration-500">
          
          {/* Status Bar */}
          <div className="bg-white p-4 rounded-xl shadow-sm border border-gray-200 mb-8 flex flex-col md:flex-row gap-6 justify-between items-start md:items-center">
            
            {/* Upload Button */}
            <div>
              <input type="file" className="hidden" ref={fileInputRef} onChange={handleFileUpload} />
              <button 
                onClick={handleUploadClick}
                className="flex items-center gap-2 bg-blue-50 text-blue-700 px-4 py-2 rounded-lg font-semibold hover:bg-blue-100 transition-colors"
              >
                <Upload size={18} />
                Upload New File
              </button>
            </div>

            {/* Active Transfers */}
            <div className="flex-1 flex flex-col md:flex-row gap-6 w-full">
              <div className="flex-1 min-w-[200px]">
                <h4 className="text-xs font-bold text-gray-400 uppercase tracking-wider mb-2">Active Uploads</h4>
                {Object.keys(activeUploads).length === 0 ? <p className="text-sm text-gray-400 italic">None</p> : 
                  Object.values(activeUploads).map((up, i) => <ProgressBar key={i} item={up} />)
                }
              </div>
              <div className="flex-1 min-w-[200px]">
                <h4 className="text-xs font-bold text-gray-400 uppercase tracking-wider mb-2">Active Downloads</h4>
                {Object.keys(activeDownloads).length === 0 ? <p className="text-sm text-gray-400 italic">None</p> : 
                  Object.values(activeDownloads).map((dl, i) => <ProgressBar key={i} item={dl} />)
                }
              </div>
            </div>
          </div>

          {/* File List */}
          <div className="bg-white rounded-xl shadow-sm border border-gray-200 overflow-hidden">
            <div className="px-6 py-4 border-b border-gray-200 bg-gray-50">
              <h2 className="text-lg font-bold">Virtual Disk Contents</h2>
            </div>
            
            {displayFiles.length === 1 ? (
              <div className="p-12 text-center text-gray-400">
                <FileIcon className="mx-auto h-12 w-12 mb-3 opacity-20" />
                <p>The virtual disk is currently empty.</p>
              </div>
            ) : (
              <ul className="divide-y divide-gray-100">
                {displayFiles.map((filename, idx) => {
                  if (filename.endsWith('.v_old') || filename === '') return null; // Skip .v_old and .v_new files in main list
                  const hasOld = allFiles.includes(`${filename}.v_old`);
                  return (
                    <li key={idx} className="p-4 sm:px-6 hover:bg-blue-50/50 transition-colors flex flex-col sm:flex-row sm:items-center justify-between gap-4">
                      
                      <div className="flex items-center gap-3">
                        <FileIcon className="text-blue-500 h-5 w-5" />
                        <span className="font-medium">{filename}</span>
                        {hasOld && <span className="px-2 py-0.5 rounded text-[10px] font-bold bg-amber-100 text-amber-700">v_old exists</span>}
                      </div>

                      <div className="flex flex-wrap items-center gap-2">
                        {/* Downloads */}
                        <button onClick={() => handleDownload(filename)} className="flex items-center gap-1 text-xs px-3 py-1.5 bg-gray-100 hover:bg-gray-200 rounded text-gray-700 font-medium transition-colors">
                          <Download size={14} /> Current
                        </button>
                        {hasOld && (
                          <button onClick={() => handleDownload(`${filename}.v_old`)} className="flex items-center gap-1 text-xs px-3 py-1.5 bg-gray-100 hover:bg-gray-200 rounded text-gray-700 font-medium transition-colors">
                            <Download size={14} /> Old
                          </button>
                        )}

                        <div className="w-px h-5 bg-gray-300 mx-1 hidden sm:block"></div>

                        {/* Deletes */}
                        <button onClick={() => handleDelete(filename, 'current')} className="flex items-center gap-1 text-xs px-3 py-1.5 border border-red-200 text-red-600 hover:bg-red-50 rounded font-medium transition-colors">
                          <Trash2 size={14} /> Curr
                        </button>
                        {hasOld && (
                          <>
                            <button onClick={() => handleDelete(filename, 'old')} className="flex items-center gap-1 text-xs px-3 py-1.5 border border-red-200 text-red-600 hover:bg-red-50 rounded font-medium transition-colors">
                              <Trash2 size={14} /> Old
                            </button>
                            <button onClick={() => handleDelete(filename, 'both')} className="flex items-center gap-1 text-xs px-3 py-1.5 bg-red-600 text-white hover:bg-red-700 rounded font-medium transition-colors">
                              <Trash2 size={14} /> Both
                            </button>
                          </>
                        )}
                      </div>
                    </li>
                  );
                })}
              </ul>
            )}
          </div>
        </div>
      )}

      {/* Footer */}
      <footer className="bg-gray-900 text-gray-400 py-12 mt-auto position-relative">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 grid md:grid-cols-2 gap-8">
          <div>
            <h3 className="text-white text-lg font-bold mb-4">Who Are We?</h3>
            <p className="text-sm mb-4 max-w-sm leading-relaxed">
              We are 2nd-year Computer Science and Engineering (CSE) students at IIT BHU. This project was developed as an academic exploration of Operating Systems and highly concurrent networking.
            </p>
            <p className="text-sm">
              <strong className="text-gray-300">Under the guidance of:</strong><br />
              Prof. Bhaskar Biswas<br />
              Head of the Department (CSE), IIT BHU
            </p>
          </div>
          <div className="md:text-right flex flex-col justify-end">
            <p className="text-sm mb-2">© {new Date().getFullYear()} Networked FS Project. All rights reserved.</p>
            <p className="text-sm">Indian Institute of Technology (BHU) Varanasi</p>
          </div>
        </div>
      </footer>
    </div>
  );
}