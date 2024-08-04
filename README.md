# flexclicker 1.21

##Description
This is a simple DLL injection autoclicker that uses JNI to communicate with Java. It is designed to be used with Lunar Client 1.21. It is written in C++ and Java.

## Prerequisites

Before building the project, ensure you have the following installed:

- **CMake** 3.30.2
- **Visual Studio 2022**
- **C++ 20**
- **JDK 21**

## Build Instructions

1. **Open CMake**

   Launch the CMake GUI application.

2. **Configure Source and Build Directories**

    - In the CMake GUI, paste the directory where you saved the project into the "Where is the source code" field.
    - Select or create a directory for the build in the "Where to build the binaries" field. It is recommended to use a subdirectory within the project directory, e.g., `<project_directory>/build`.

3. **Generate Build Files**

    - Click the "Generate" button.
    - If prompted that the build directory does not exist, click "Yes" to create it.

4. **Open Project in Visual Studio**

    - Once the generation is complete, open the generated `.sln` (solution) file in Visual Studio.

5. **Set Startup Project**

    - In the Solution Explorer, right-click on `autoclicker` and select "Set as Startup Project".

6. **Build the Project**

    - Build the project by pressing `F5` or selecting "Build" -> "Build Solution" from the menu.

7. **Locate the DLL**

    - After the build is successful, navigate to the build directory: `<project_directory>/build/releases`.
    - The `autoclicker.dll` file will be located there.

8. **Inject the DLL**

    - Use a tool such as **Process Hacker** to inject the `autoclicker.dll` into your `javaw.exe` process.

## Notes

- Ensure that you have the correct version of all prerequisites installed to avoid compatibility issues.
- If you encounter any errors during the build process, double-check the installation and configuration of your prerequisites.
