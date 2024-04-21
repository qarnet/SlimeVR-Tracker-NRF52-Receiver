# Requierements

Install the J-Link Software and Documentation Pack: https://www.segger.com/downloads/jlink/#J-LinkSoftwareAndDocumentationPack  
Install VS Code: https://code.visualstudio.com/download  
Install the nRF Command line tools: https://www.nordicsemi.com/Products/Development-tools/nRF-Command-Line-Tools/Download  
Install the nRF Connect SDK Desktop application: https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-Desktop/Download?lang=en#infotabs  
Install the toolchain manager in the desktop application.  
Install nRF Connect SDK 2.6.0 inside the toolchain manager.  
Once nRF Connect SDK is installed, press open VS Code in the toolchain and install the missing extensions.  

# To compile

Open the Project in VS Code.  
Click on the nRF Connect extension.  
Click on add a build configuration and choose the xiao_ble board under all boards.  
Press Build Configuration.  

# To upload the firmware

Double press the reset button on the xiao ble.  
Drag and drop the .uf2 file from build/zephyr into the newly appeared folder.  

# Debugging

Open Serial port to ACM device with baudrate of 230400  
