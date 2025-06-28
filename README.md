# 🖱️ Lunar AutoClicker – 1.8 PvP Style (1.16.5+)

Simple, clean autoclicker for 1.8-style PvP, made to work with **Lunar Client** (tested on version 1.21.4).  
Designed with a lightweight GUI – just build, inject, and click away.

---

## ✅ Features

- Works with **Minecraft 1.16.5+**
- Designed for **1.8 combat mechanics**
- Compatible with **Lunar Client**

---

## 🛠️ What You Need

Before building, make sure you have:

- Windows PC  
- Visual Studio (2022 or newer)  
- CMake (to generate the project)  
- Java installed  

> 🔧 Make sure `JAVA_HOME` is set (see below)

---

## 💡 Setup Guide (No Terminal Needed)

### 1. Install Java

- Download the latest Java JDK from [Oracle](https://www.oracle.com/java/technologies/javase-downloads.html) or [Adoptium](https://adoptium.net/)
- After installing, set the environment variable:

  1. Press `Win + S` → search **"Edit the system environment variables"**
  2. Click **Environment Variables**
  3. Under “System variables,” click **New**:
     - Name: `JAVA_HOME`
     - Value: `C:\Path\To\Your\Java\jdk`

---

### 2. Download and Open the Project

- Clone or download the repo as a ZIP  
- Open the folder in **Visual Studio**

---

### 3. Generate with CMake GUI

- Open the **CMake GUI** (comes with CMake)
- Set:
  - **Source code:** path to your project folder
  - **Build folder:** `build/` inside that folder
- Click **Configure** → choose **Visual Studio 2022 (x64)**
- Click **Generate**
- Then click **Open Project**

---

### 4. Build the Project

In **Visual Studio**:

- Set build mode to **Release**
- Right-click the **Solution** → **Build All**
- You’ll find the `.exe` and `.dll` in `build/Release/`

---

## 🚀 How to Use

1. Copy the built `.exe` and `.dll` to the **same folder**
2. Start **Lunar Client** and load into a world or server
3. Use the `.exe` you built to **inject the DLL into Lunar Client**
4. To set your CPS, change the value in `AutoclickerModule.h` and rebuild

---

## ❗ Important Notes

- Use this in **offline or private environments only**
- May trigger **anti-cheat** on public servers
- Built for **educational and testing purposes only**
