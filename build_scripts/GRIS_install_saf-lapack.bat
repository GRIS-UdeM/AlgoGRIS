@echo off
setlocal

set "ONEAPI_VARS=C:\Program Files (x86)\Intel\oneAPI\compiler\2026.1\env\vars.bat"
set "NINJA=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
set "IFX=C:\Program Files (x86)\Intel\oneAPI\compiler\2026.1\bin\ifx.exe"
set "SRC=%~dp0..\submodules\Spatial_Audio_Framework\lapack"
set "PREFIX=%SRC%\install"

call "%ONEAPI_VARS%" || exit /b 1

cmake -S "%SRC%" -B "%SRC%\build" -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_MAKE_PROGRAM="%NINJA%" ^
  -DCMAKE_Fortran_COMPILER="%IFX%" ^
  -DBUILD_SHARED_LIBS=OFF ^
  -DLAPACKE=ON ^
  -DCMAKE_INSTALL_PREFIX="%PREFIX%" || exit /b 1

cmake --build "%SRC%\build" || exit /b 1

cmake -S "%SRC%" -B "%SRC%\build-debug" -G Ninja ^
  -DCMAKE_BUILD_TYPE=Debug ^
  -DCMAKE_MAKE_PROGRAM="%NINJA%" ^
  -DCMAKE_Fortran_COMPILER="%IFX%" ^
  -DBUILD_SHARED_LIBS=OFF ^
  -DLAPACKE=ON ^
  -DCMAKE_INSTALL_PREFIX="%PREFIX%" || exit /b 1

cmake --build "%SRC%\build-debug" || exit /b 1

endlocal