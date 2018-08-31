# Visual Studio Plugin
add vs command to reflection file! 
**Install**
Please set src\CommandImplementation\Resources\tinyrefl.zip
tinyrefl.zip include :
  1.tinyrefl.exe
  2.clang.exe
  3.fmt.dll
  4.libclang.dll
  5.directory(llvm include dir): \clang\6.0.0\include


**Note**
Only support reflection *.h or *.hpp header file,and file must to in Project(if not in project,plugin will use first project's Include and Defines property).

command on context menu,just key down mouse right button in file.
