:: "Batch script to build and install a custom MKL library for SAF. Run using: x64 Command Prompt for VS 2017." 
::
@echo off
cd /d "%~dp0\..\submodules\Spatial_Audio_Framework\scripts"

CLS

ECHO ******************************************************************************************
ECHO ************** Custom MKL library installer for the Spatial_Audio_Framework **************
ECHO ******************************************************************************************
ECHO.
ECHO This batch script will build the required saf_mkl_custom files
ECHO and copy them into:
ECHO   - "Spatial_Audio_Framework/dependencies/Win64/lib/saf_mkl_custom_lp64.lib" 
ECHO   - "AlgoGRIS/saf_custom_libs/saf_mkl_custom_lp64.dll".  
ECHO You may choose between sequential and threaded versions of the library. Sequential is the
ECHO recommended option.
ECHO.
ECHO NOTE: you will need to run this script using the "x64 Native Tools Command Prompt for VS [year].exe", which 
ECHO will require Administrator privileges!
ECHO.

:: ========================================================================= ::
:: Check that MKL is installed
IF NOT EXIST "C:\Program Files (x86)\Intel\oneAPI\mkl\latest\share\mkl\tools\builder" (
    echo Intel MKL not installed on this machine. Note that Intel MKL can be freely downloaded from here:
    echo "https://software.intel.com/content/www/us/en/develop/tools/oneapi/base-toolkit/download.html"
    EXIT /B
)
:: Copy saf_mkl_list to MKL builder folder
echo Copying saf_mkl_list into MKL builder folder
xcopy "saf_mkl_list" "C:\Program Files (x86)\Intel\oneAPI\mkl\latest\share\mkl\tools\builder"

:: ========================================================================= ::
ECHO.
ECHO Building sequential versions of the custom MKL library (LP64 interface) for SAF...
cd /d  C:/Program Files (x86)/Intel/oneAPI/mkl/latest/share/mkl/tools/builder
nmake intel64 interface=lp64 threading=sequential name=saf_mkl_custom_lp64 export=saf_mkl_list
ECHO.
ECHO Copying files to correct folders...
cd /d "%~dp0\..\submodules\Spatial_Audio_Framework"
mkdir "dependencies\Win64\lib"
xcopy "C:\Program Files (x86)\Intel\oneAPI\mkl\latest\share\mkl\tools\builder\saf_mkl_custom_lp64.lib" "dependencies\Win64\lib" 
xcopy "C:\Program Files (x86)\Intel\oneAPI\mkl\latest\share\mkl\tools\builder\saf_mkl_custom_lp64.dll" "..\..\saf_custom_libs"

cd /d "%~dp0
