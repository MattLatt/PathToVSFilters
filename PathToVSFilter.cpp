#include <boost/filesystem.hpp>

#include <boost/thread.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <boost/format.hpp>

#include <boost/program_options.hpp>

#include <set>
#include <map>
#include <exception>
#include <vector>
#include <iostream>
#include <fstream>
#include <cstring>

namespace bfs = boost::filesystem;
namespace bpo = boost::program_options; 

//!Vector type to store path objects
typedef std::vector<bfs::path> bfs_path_vec; 
//!stdout mutex
boost::mutex output_mutex;
//vector of all headers path founded
bfs_path_vec *headers = NULL;
//vector of all headers path founded
bfs_path_vec *sources = NULL;

//list of existing filter with theire XML node string.
//To do : Convert to vector as not needing the XML node 
std::map<std::string,std::string> filter_map;
//headers extension to look for 
//To do : convert to local param
std::vector<std::string> headers_ext;
//sources extension to look for 
//To do : convert to local param
std::vector<std::string> sources_ext;

//!Create a recursive iterator over a path object
bfs::recursive_directory_iterator createRIterator(bfs::path path);

//!List recursively the content of a folder
void plainListTree(bfs::path path/*, bfs_path_vec *PathVec*/);
//!Listing the content of a folder
void ListFolderContent (bfs::path Folder/*, bfs_path_vec *GlobalVector*/);
//!Dump a Path Vector
void DumpVector(bfs_path_vec *Vector);

//!Dump a Path Vector to XML
void ToXML(std::string output_path, bfs_path_vec *headers_vec, bfs_path_vec *sources_vec, bfs::path root_headers, bfs::path root_sources, bfs::path base);
//!format std string with %s using boost format
std::string format_string(const std::string& format_string, const std::vector<std::string>& args);
//!make a XML Filter node name
std::string make_filter_name(bfs::path root, bfs::path parent, std::string filter_base);
//!make a XML Filter node 
std::string make_filter_item(std::string filter_name, std::string filter_str, std::string filter_ext, boost::uuids::basic_random_generator<boost::mt19937> gen);
//!make a XML ClInclude node
std::string make_ClInclude_item(std::string filter_name, std::string clinclude_str, std::vector<std::string> args, bfs::path relative_file);
//!process files founded
void process_path(bfs_path_vec *path_vec, std::string cl_item_str, std::string filter_str, 
				  bfs::path root_path, bfs::path base_path, std::string filter_base,
				  boost::uuids::basic_random_generator<boost::mt19937> gen, 
				  std::string &cl_item_group, std::string &filter_item_group);


//####################################################################
//!Main Program
//####################################################################
int main (int argc, char* argv[])
	{
	std::string headers_root, sources_root, project_root, out_xml;

    bpo::options_description usage("Options"); 
    usage.add_options() 
      ("help,h", "Print help message") 
      ("headers_path,H", bpo::value<std::string>(&headers_root)->required(), "folder where to look for headers. Mandatory") 
      ("sources_path,S", bpo::value<std::string>(&sources_root), "folder where to look for sources. if not specified the same as headers") 
      ("project_root,P", bpo::value<std::string>(&project_root)->required(), "folder where to the wokspace (of which we are building the filter) will be, to get relative path. Mandatory") 
      ("output_filter,O", bpo::value<std::string>(&out_xml)->required(), "output filname of the filter. Mandatory"); 
 
    bpo::variables_map vm; 
 
    try 
		{ 
		bpo::store(bpo::parse_command_line(argc, argv, usage),vm); // throws on error 
 
		if ( vm.count("help")  ) 
			{ 
			std::cout << "Usage:" << std::endl;
			std::cout << usage << std::endl;
			return 0; 
			} 
 
		bpo::notify(vm); // throws on error, so do after help in case 
		} 
    catch(boost::program_options::required_option& e) 
		{
		std::cerr << "Error: " << e.what() << std::endl << std::endl; 
		std::cerr << usage << std::endl;
		return 1; 
		} 
    catch(boost::program_options::error& e) 
		{ 
		std::cerr << "Error: " << e.what() << std::endl << std::endl; 
		std::cerr << usage << std::endl;
		return 2; 
		} 

	bfs::path root_path_headers(headers_root);
	bfs::path root_path_sources(sources_root);
	bfs::path base_path(project_root);

	std::cout << "Listing Content of Path : " << root_path_headers ;
	if( sources_root != "")
		{ std::cout << "; " << root_path_sources;} 
	std::cout << std::endl;

	headers = new bfs_path_vec();
	sources = new bfs_path_vec();

	//To do : allow user to specify extensions filter to exe by argv even better using regexp instead of vector of ext...
	headers_ext.push_back(".h");
	headers_ext.push_back(".hpp");
	headers_ext.push_back(".inl");

	sources_ext.push_back(".c");
	sources_ext.push_back(".cpp");

	ListFolderContent(root_path_headers);
	if ( root_path_sources != "" )
		{ ListFolderContent(root_path_sources);}
	else
		{ root_path_sources = root_path_headers; }

	ToXML (out_xml, headers, sources, root_path_headers, root_path_sources, base_path);

	return 0;
	}

//********************************************************************
//!Create a recursive iterator over a path object
//********************************************************************
bfs::recursive_directory_iterator createRIterator(bfs::path path)
	{
    try
		{ return bfs::recursive_directory_iterator(path); }
    catch(bfs::filesystem_error& fex)
		{
        std::cout << fex.what() << std::endl;
        return bfs::recursive_directory_iterator();
		}
	}


//********************************************************************
//!List recursively the content of a folder
//********************************************************************
void plainListTree(bfs::path path)
	{
    bfs::recursive_directory_iterator it = createRIterator(path);
    bfs::recursive_directory_iterator end;
 
	bfs_path_vec Curpath;

    while(it != end)
		{
		boost::mutex::scoped_lock lock(output_mutex);

		if (std::find(headers_ext.begin(), headers_ext.end(), (*it).path().extension()) != headers_ext.end())
			{ headers->push_back((*it).path() );}
		if (std::find(sources_ext.begin(), sources_ext.end(), (*it).path().extension()) != sources_ext.end())
			{ sources->push_back((*it).path() );}

        if(bfs::is_directory(*it) && bfs::is_symlink(*it))
            {it.no_push();}
 
        try
			{ ++it; }
        catch(std::exception& ex)
			{
            std::cout << ex.what() << std::endl;
            it.no_push();
            try 
				{ ++it; } 
			catch(...)
				{ std::cout << "!!" << std::endl; return; }
			}
		}
	}


//***********************************************************
//!Listing the content of a folder
//***********************************************************
void ListFolderContent (bfs::path Folder)
	{
     bfs_path_vec v; // so we can sort them later
	 boost::thread_group tg;

     std::copy(bfs::directory_iterator(Folder), bfs::directory_iterator(), std::back_inserter(v));
     std::sort(v.begin(), v.end());       // sort, since directory iteration
                                          // is not ordered on some file systems
  
     for (bfs_path_vec::const_iterator it (v.begin()); it != v.end(); ++it)
        {
		if (std::find(headers_ext.begin(), headers_ext.end(), (*it).extension()) != headers_ext.end())
			{ headers->push_back(*it);}
		if (std::find(sources_ext.begin(), sources_ext.end(), (*it).extension()) != sources_ext.end())
			{ sources->push_back(*it);}

		if( bfs::is_directory(*it) )
			{ tg.create_thread(boost::bind(plainListTree, *it)); }
        }
	 tg.join_all();
	}


//********************************************************************
//!Dump a Path Vector
//********************************************************************
void DumpVector(bfs_path_vec *Vector)
	{
	for (bfs_path_vec::const_iterator it (Vector->begin()); it != Vector->end(); ++it)
		{ std::cout << (*it).string() << std::endl; }
	}

//********************************************************************
//!Dump a Path Vector to XML
//********************************************************************
void ToXML(std::string output_path, bfs_path_vec *headers_vec, bfs_path_vec *sources_vec, bfs::path root_headers, bfs::path root_sources, bfs::path base)
	{
	boost::mt19937 ran;
	ran.seed(time(NULL)); // one should likely seed in a better way
	boost::uuids::basic_random_generator<boost::mt19937> gen(&ran);
	
	std::string header = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<Project ToolsVersion=\"4.0\" xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\">\n";
	std::string filter_default_str = "";
	//Filter node 
	std::string filter_str = "\t\t<Filter Include=\"%s\">\n\t\t\t<UniqueIdentifier>{%s}</UniqueIdentifier>%s\n\t\t</Filter>"; //filter_include + UUID
	//ClInclude node (headers)
	std::string clinclude_str = "\t\t<ClInclude Include=\"%s\">\n\t\t\t<Filter>%s</Filter>\n\t\t</ClInclude>"; //filter_include + .h filename
	//ClCompile node (sources)
	std::string clcompile_str = "\t\t<ClCompile Include=\"%s\">\n\t\t\t<Filter>%s</Filter>\n\t\t</ClCompile>"; //filter_include + .cpp filename

	//building default filter of a VS 2010 Workspace
	filter_default_str += make_filter_item("Source Files", filter_str, "\n\t\t\t<Extensions>cpp;c;cc;cxx;def;odl;idl;hpj;bat;asm;asmx</Extensions>", gen);
	filter_default_str += make_filter_item("Header Files", filter_str, "\n\t\t\t<Extensions>h;hpp;hxx;hm;inl;inc;xsd</Extensions>", gen);
	filter_default_str += make_filter_item("Resource Files", filter_str, "\n\t\t\t<Extensions>rc;ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe;resx;tiff;tif;png;wav;mfcribbon-ms</Extensions>", gen);

	//Adding start balise for all the groups
	std::string filter_item_group = "\t<ItemGroup>\n";
	filter_item_group += filter_default_str;
	std::string clinclude_item_group = "\t<ItemGroup>\n";
	std::string clcompile_item_group = "\t<ItemGroup>\n";

	//Convert vector of headers paths to XML
	process_path(headers_vec, clinclude_str, filter_str, root_headers, base, "Header Files", gen, clinclude_item_group, filter_item_group);
	//Convert vector of sources paths to XML
	process_path(sources_vec, clcompile_str, filter_str, root_sources, base, "Source Files", gen, clcompile_item_group, filter_item_group);

	//Closing all group nodes
	filter_item_group += "\t</ItemGroup>\n";
	clinclude_item_group += "\t</ItemGroup>\n";
	clcompile_item_group  += "\t</ItemGroup>\n";

	std::ofstream out_xml;
	
	out_xml.open (output_path.c_str());
	out_xml << header;
	out_xml << filter_item_group;
	out_xml << clcompile_item_group;
	out_xml << clinclude_item_group;
	out_xml << "</Project>";
	out_xml.close();

	}


//********************************************************************
//!format a std::string
//********************************************************************
std::string format_string(const std::string& format_string, const std::vector<std::string>& args)	
	{	
    boost::format f(format_string);
    for (std::vector<std::string>::const_iterator it = args.begin(); it != args.end(); ++it) 
		{ f % *it; }
    return f.str();
	}



//********************************************************************
//!make a XML Filter node name
//********************************************************************
std::string make_filter_name(bfs::path root, bfs::path parent, std::string filter_base)
	{
	//std::string filter_include = "Header Files";

	if ( root != parent)
		{
		bfs::path::iterator itr(root.begin()), itb(parent.begin());
		bfs::path::iterator itre(root.end()), itbe(parent.end());

		//Iterate to the last common path of both iterators
		while ((*itr).stem() == (*itb).stem())
			{ ++itr, ++itb; }

		while (itb != itbe)
			{
			filter_base += "\\" + (*itb).stem().string();
			++itb;
			}
		}

	return filter_base;
	}

//********************************************************************
//!make a XML Filter node 
//********************************************************************
std::string make_filter_item(std::string filter_name, std::string filter_str, std::string filter_ext, boost::uuids::basic_random_generator<boost::mt19937> gen)
	{
	std::string filter_item = "";
	std::vector<std::string> args;

	boost::uuids::uuid u = gen();
	args.push_back(filter_name);
	args.push_back(boost::uuids::to_string(u));
	args.push_back(filter_ext);

	filter_item = format_string(filter_str,args) + "\n"; 

	return filter_item;
	}

//********************************************************************
//!make a XML ClInclude node
//********************************************************************
std::string make_ClInclude_item(std::string filter_name, std::string clinclude_str, bfs::path relative_file)
	{
	std::string ClInclude_item = "";
	std::vector<std::string> args;

	args.push_back(relative_file.string());
	args.push_back(filter_name);
	ClInclude_item = format_string(clinclude_str,args) + "\n"; 

	return ClInclude_item;
	}

//********************************************************************
//!process files founded
//********************************************************************
void process_path(bfs_path_vec *path_vec, std::string cl_item_str, std::string filter_str, 
				  bfs::path root_path, bfs::path base_path, std::string filter_base,
				  boost::uuids::basic_random_generator<boost::mt19937> gen, 
				  std::string &cl_item_group, std::string &filter_item_group)
	{
	std::vector<std::string> args;

	for (bfs_path_vec::const_iterator it (path_vec->begin()); it != path_vec->end(); ++it)
		{
		bfs::path cur_path= (*it);

		if ( cur_path.parent_path() == root_path)
			{
			args.push_back(cur_path.filename().string());
			args.push_back(filter_base);//"Header Files");
			//std::string formated_clinclude = format_string(clinclude_str,args); 
			cl_item_group += format_string(cl_item_str,args) + "\n"; 
			}
		else
			{
			bfs::path parent = cur_path.parent_path();
			//bfs::path relative_path = bfs::relative(root_path, cur_path.);
			bfs::path relative_file = bfs::relative(cur_path,base_path);

			std::string filter_name = make_filter_name(root_path, parent, filter_base);
			std::string filter_name_order1 = filter_name;

			while ( filter_map[filter_name] == "" && filter_name != filter_base)
				{ 
				std::string filter_node = make_filter_item(filter_name, filter_str, "", gen);
				filter_map[filter_name] = filter_node;
				filter_item_group += filter_node;

				parent = parent.parent_path();
				filter_name = make_filter_name(root_path, parent, filter_base);
				}

			cl_item_group += make_ClInclude_item(filter_name_order1,cl_item_str, relative_file); 
			}
		}

	}
