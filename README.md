# Camera Usage Viewer (Win32)
![screenshot](https://github.com/bob-paydar/CamUsageWin/blob/main/Screenshot.png)
A lightweight **Windows Desktop (Win32)** application written in C++ that shows which applications are using (or recently used) your laptop webcam.  

The program reads Windows’ privacy usage registry (`CapabilityAccessManager → ConsentStore → webcam`) and displays a live list of applications with their usage status.

---

## ✨ Features

- 📋 **ListView interface** with full details:
  - Kind: `Desktop` (classic EXE) or `Packaged` (Microsoft Store/UWP app)  
  - App name and executable path  
  - Active status (`Yes`/`No`)  
  - Last Start and Last Stop timestamps (converted to local time)
- 🔄 **Refresh button** to reload usage instantly
- ✅ **Current only filter** (checkbox) to show only apps currently accessing the camera
- 📌 **Status bar** showing `Ready - Bob Paydar`
- 🔒 Uses Windows privacy API registry values (no third-party libraries required)

---

## 🛠️ Build Instructions (Visual Studio 2022)

1. Open **Visual Studio 2022**  
2. Go to **File → New → Project → Windows Desktop Application (C++)**  
3. Choose **Empty Project**, name it (e.g., `CamUsageWin`), and click **Create**  
4. Right-click **Source Files → Add → New Item → C++ File (.cpp)**  
5. Name it `CamUsageWin.cpp` and paste the provided source code  
6. Project settings:
   - **Configuration Properties → General → Character Set** → `Use Unicode Character Set`
   - **C/C++ → Language → C++ Language Standard** → `ISO C++17` (or later)
   - **Linker → System → SubSystem** → `Windows (/SUBSYSTEM:WINDOWS)`
7. Build and run (`Ctrl + F5`)

---

## 🚀 Usage

- Run the compiled application  
- The main window will display:
  - All packaged apps and desktop apps that accessed the camera  
  - Whether they are **currently active**  
  - Last start/stop times in local time zone  
- Click **Refresh** anytime  
- Check **Current only** to filter to active apps  

---

## 📂 Registry Path Used

The application queries:

```
HKCU\Software\Microsoft\Windows\CurrentVersion\CapabilityAccessManager\ConsentStore\webcam
```

- **Packaged apps** appear directly under this key  
- **Desktop apps** appear under `NonPackaged` with their EXE path encoded using `#` instead of `\`  

`LastUsedTimeStart` and `LastUsedTimeStop` values are used to determine usage.  

---

## 📸 Screenshot (placeholder)

```
+---------+-----------------------+----------------------------------+--------+-------------------+-------------------+
| Kind    | App                   | EXE                              | Active | Last Start        | Last Stop         |
+---------+-----------------------+----------------------------------+--------+-------------------+-------------------+
| Desktop | obs64.exe             | C:\Program Files\OBS\bin\obs64.exe | Yes   | 2025-09-20 10:15 |                   |
| Packaged| Microsoft.WindowsCamera|                                  | No     | 2025-09-18 09:10 | 2025-09-18 09:12  |
+---------+-----------------------+----------------------------------+--------+-------------------+-------------------+
```

---

## 📖 Notes & Limitations

- Windows only logs usage if **Camera access control** is enabled under **Settings → Privacy & Security → Camera**  
- “Active” is inferred: if `LastUsedTimeStop == 0`, the app is considered still using the camera  
- Background services and some drivers may not appear in this list  

---

## 👨‍💻 Author

Programmer: **Bob Paydar**

---

## 📜 License

This project is released under the MIT License. See [LICENSE](LICENSE) for details.
