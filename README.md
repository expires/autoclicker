🖱️ Lunar AutoClicker – 1.8 PvP Style (1.16.5+)
Simple, clean autoclicker for 1.8-style PvP, made to work with Lunar Client (tested on version 1.21.4).
Designed with a lightweight GUI – just build, inject, and click away.

✅ Features
Works with Minecraft 1.16.5+

Designed for 1.8 combat mechanics

Compatible with Lunar Client

🛠️ What You Need
Before building, make sure you have:

Windows PC

Visual Studio (2022 or newer)

CMake (to generate the project)

Java installed

Make sure JAVA_HOME is set (see below)

💡 Setup Guide (No Terminal Needed)
Install Java

Download the latest Java JDK from Oracle or Adoptium

After installing, set the environment variable:

Press Win + S → search "Edit the system environment variables"

Click Environment Variables

Under “System variables,” click New:

Name: JAVA_HOME

Value: C:\Path\To\Your\Java\jdk

Download and Open the Project

Clone or download the repo as a ZIP

Open the folder in Visual Studio

Generate with CMake GUI

Open the CMake GUI (comes with CMake)

Set:

Source code: path to your project folder

Build folder: build/ inside that folder

Click Configure → choose Visual Studio 2022 (x64)

Click Generate

Then click Open Project

Build the Project

In Visual Studio:

Set build mode to Release

Right-click the Solution → Build All

You’ll find the .exe and .dll in build/Release/

🚀 How to Use
Copy the built .exe and .dll to the same folder

Start Lunar Client and load into a world or server

use the exe built to inject the DLL into Lunar Client

To set your CPS change this in the AutoclickerModule.h file

❗ Important Notes
Use this in offline or private environments only

May trigger anti-cheat on public servers

Built for educational and testing purposes
