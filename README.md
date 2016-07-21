# PathToVSFilters
List all the includes and sources files from a folder and build a Visual Studio 2010 filter XML file "*.vcxproj.filters".

The Visual Studio Filter file allow to sort file of the workspace in the "Solution Explorer" tab 
so it's easier to read.

That program convert the folder tree to a Visual studio 2010 Filter file.

It use boost library (boost_filesystem, boost_thread, boost_format, boost_uuid and boost_program_options).
I do not provide the boost lib but you can download them here: 
https://sourceforge.net/projects/boost/files/boost/1.61.0/boost_1_61_0.zip

In the Visual Studio worksapce settings, you will have to change boost path into the "Additional Include Directories".

You can download to the MS VC2010 binaries here:
https://sourceforge.net/projects/boost/files/boost-binaries/1.61.0

##2016/07/21 :
	- first release, tested with libgeos 3.5.0
	
Evolution :
	- filtering the headers and sources extension to add in the output XML by programm arguments.

	- filtering the headers and sources extension to add in the output XML by regexp ?
	
	- remove all the global variables and build a FileFinder class.
	
	- adding a code::blocks workspace
	
