/* -*- mode: c++; coding: utf-8; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; show-trailing-whitespace: t -*- vim:fenc=utf-8:ft=tcl:et:sw=4:ts=4:sts=4*/

#ifndef __toolboxbloodflowmesh_H
#define __toolboxbloodflowmesh_H 1

#include <angiotkMeshingConfig.h>

#include <feel/feelfilters/gmsh.hpp>
#if defined( FEELPP_HAS_GMSH_H )

#include <GmshConfig.h>
#include <Gmsh.h>
#include <GModel.h>
#include <OpenFile.h>
#include <GmshDefines.h>
#include <Context.h>
#endif

//#define MY_PYTHON_EXECUTABLE /opt/local/bin/python2.7

namespace Feel
{

class ToolBoxCenterlines
{
public :
    ToolBoxCenterlines( std::string prefix )
        :
        M_prefix( prefix ),
        M_inputPath( soption(_name="input.filename",_prefix=this->prefix()) ),
        M_outputDirectory( soption(_name="output.directory",_prefix=this->prefix()) ),
        M_forceRebuild( boption(_name="force-rebuild",_prefix=this->prefix() ) )
        {
            std::vector<int> sids,tids;
            if ( Environment::vm().count(prefixvm(this->prefix(),"source-ids").c_str()) )
                 sids = Environment::vm()[prefixvm(this->prefix(),"source-ids").c_str()].as<std::vector<int> >();
            if ( Environment::vm().count(prefixvm(this->prefix(),"target-ids").c_str() ) )
                 tids = Environment::vm()[prefixvm(this->prefix(),"target-ids").c_str()].as<std::vector<int> >();
            for ( int id : sids )
                M_sourceids.insert( id );
            for ( int id : tids )
                M_targetids.insert( id );

            if ( !M_inputPath.empty() && M_outputPath.empty() )
            {
                this->updateOutputPathFromInputFileName();
                //this->updateOutputFileName();
                /*if ( M_outputDirectory.empty() )
                    this->updateOutputPathFromInputPath();
                else
                this->updateOutputPathFromInputFileName();*/
            }
        }

    ToolBoxCenterlines( ToolBoxCenterlines const& e )
        :
        M_prefix( e.M_prefix ),
        M_inputPath( e.M_inputPath ),
        M_outputPath( e.M_outputPath ),
        M_outputDirectory( e.M_outputDirectory ),
        M_targetids( e.M_targetids ),
        M_sourceids( e.M_sourceids ),
        M_forceRebuild( e.M_forceRebuild )
        {}

    std::string prefix() const { return M_prefix; }
    WorldComm const& worldComm() const { return Environment::worldComm(); }

    std::string inputPath() const { return M_inputPath; }
    //std::string setFileNameSTL(std::string s) { return M_inputPath=s;this->updateOutputFileName(); }
    void setStlFileName(std::string s) { M_inputPath=s; this->updateOutputFileName(); }

    std::string outputPath() const { return M_outputPath; }

    std::set<int> const& targetids() const { return M_targetids; }
    std::set<int> const& sourceids() const { return M_sourceids; }
    void setTargetids( std::set<int> const& s ) { M_targetids=s; }
    void setSourceids( std::set<int> const& s ) { M_sourceids=s; }

    bool forceRebuild() const { return M_forceRebuild; }

    void updateOutputFileName()
    {
        std::string outputName = fs::path(this->inputPath()).stem().string()+"_centerlines.vtk";
        //std::string outputName = fs::path(this->inputPath()).string()+"_centerlines.vtk";
        M_outputPath = (fs::current_path()/outputName).string();
    }

    void updateOutputPathFromInputFileName()
    {
        CHECK( !M_inputPath.empty() ) << "input path is empty";

        // define output directory
        fs::path meshesdirectories = Environment::rootRepository();
        if ( M_outputDirectory.empty() )
        {
            //meshesdirectories /= fs::path("/angiotk/meshing/centerlines-garbage/" + this->prefix() );
            meshesdirectories = fs::current_path();
        }
        else meshesdirectories /= fs::path(M_outputDirectory);

        // get filename without extension
        fs::path gp = M_inputPath;
        std::string nameMeshFile = gp.stem().string();

        std::string newFileName = (boost::format("%1%_centerlines.vtk")%nameMeshFile ).str();
        fs::path outputPath = meshesdirectories / fs::path(newFileName);
        M_outputPath = outputPath.string();
    }


    void run()
    {
        if ( !fs::exists( this->inputPath() ) )
        {
            if ( this->worldComm().isMasterRank() )
                std::cout << "WARNING : centerlines computation not done because this input path not exist :" << this->inputPath() << "\n";
            return;
        }
        if ( this->sourceids().empty() && this->targetids().empty() )
        {
            if ( this->worldComm().isMasterRank() )
                std::cout << "WARNING : centerlines computation not done because this sourceids and targetids are empty\n";
            return;
        }


        std::ostringstream coutStr;

        coutStr << "\n"
                  << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n"
                  << "---------------------------------------\n"
                  << "run ToolBoxCenterlines \n"
                  << "---------------------------------------\n";
        coutStr << "inputPath          : " << this->inputPath() << "\n";
        coutStr << "targetids : ";
        for ( int id : this->targetids() )
            coutStr << id << " ";
        coutStr << "\n";
        coutStr << "sourceids : ";
        for ( int id : this->sourceids() )
            coutStr << id << " ";
        coutStr << "\n"
                << "output path       : " << this->outputPath() << "\n"
                << "---------------------------------------\n"
                << "---------------------------------------\n";
        std::cout << coutStr.str();

        if ( fs::exists( this->outputPath() ) && !this->forceRebuild() )
        {
            std::cout << "already file exist, ignore centerline\n"
                      << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n";
            return;
        }

        fs::path directory;
        // build directories if necessary
        if ( !this->outputPath().empty() && this->worldComm().isMasterRank() )
        {
            directory = fs::path(this->outputPath()).parent_path();
            if ( !fs::exists( directory ) )
                fs::create_directories( directory );
        }
        // // wait all process
        this->worldComm().globalComm().barrier();


        //fs::path stlNamePath = fs::path(this->inputPath());
        fs::path outputFileNamePath = fs::path(this->outputPath());
        std::string name = outputFileNamePath.stem().string();

        std::string pathVTP = (directory/fs::path(name+".vtp")).string();

        std::ostringstream __str;

        // source ~/packages/vmtk/vmtk.build2/Install/vmtk_env.sh
        std::string pythonExecutable = BOOST_PP_STRINGIZE( PYTHON_EXECUTABLE );

        __str << pythonExecutable << " ";
        std::string dirBaseVmtk = BOOST_PP_STRINGIZE( VMTK_BINARY_DIR );
        __str << dirBaseVmtk << "/vmtk " << dirBaseVmtk << "/vmtkcenterlines "
              <<"-seedselector profileidlist ";
        __str << "-targetids ";
        for ( int id : this->targetids() )
            __str << id << " ";
        __str << "-sourceids ";
        for ( int id : this->sourceids() )
            __str << id << " ";

        __str << "-ifile " << this->inputPath() << " ";
        __str << "-ofile " << pathVTP << " " //name << ".vtp "
              << "--pipe " << dirBaseVmtk << "/vmtksurfacewriter "
              << "-ifile " << pathVTP << " " //name << ".vtp "
              << "-ofile " << this->outputPath() << " " //name << ".vtk "
              << " -mode ascii ";

        std::cout << "---------------------------------------\n"
                  << "run in system : \n" << __str.str() << "\n"
                  << "---------------------------------------\n";
        auto err = ::system( __str.str().c_str() );

        std::cout << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n\n";

    }

    static
    po::options_description
    options( std::string const& prefix )
    {
        po::options_description myCenterlinesOptions( "Centerlines from STL for blood flow mesh options" );
        myCenterlinesOptions.add_options()
            (prefixvm(prefix,"input.filename").c_str(), po::value<std::string>()->default_value( "" ), "(string) input filename" )
            (prefixvm(prefix,"output.directory").c_str(), Feel::po::value<std::string>()->default_value(""), "(string) output directory")
            (prefixvm(prefix,"source-ids").c_str(), po::value<std::vector<int> >()->multitoken(), "(vector of int) source ids" )
            (prefixvm(prefix,"target-ids").c_str(), po::value<std::vector<int> >()->multitoken(), "(vector of int) target ids" )
            (prefixvm(prefix,"force-rebuild").c_str(), Feel::po::value<bool>()->default_value(false), "(bool) force-rebuild")
            ;
        return myCenterlinesOptions;
    }

private :
    std::string M_prefix;
    std::string M_inputPath, M_outputPath;
    std::string M_outputDirectory;
    std::set<int> M_targetids, M_sourceids;
    bool M_forceRebuild;
}; // class ToolBoxCenterlines

namespace detail
{
    void generateMeshFromGeo( std::string inputGeoName,std::string outputMeshName,int dim/*,std::string formatExportMesh = "msh"*/)
    {
        //fs::path outputMeshNamePath=fs::path(Environment::findFile(outputMeshName));
        fs::path outputMeshNamePath=fs::path(outputMeshName);
        std::string _name = outputMeshNamePath.stem().string();
        //std::cout << "\n _name " << _name << "\n";
        //std::cout << " _ext " << outputMeshNamePath.extension() << "\n";

        GmshInitialize();

        GModel * M_gmodel = new GModel();
        M_gmodel->setName( _name );
#if !defined( __APPLE__ )
        M_gmodel->setFileName( _name );
#endif
        M_gmodel->readGEO( inputGeoName );
        M_gmodel->mesh( dim );

        CHECK(M_gmodel->getMeshStatus() > 0)  << "Invalid Gmsh Mesh, Gmsh status : " << M_gmodel->getMeshStatus() << " should be > 0. Gmsh mesh cannot be written to disk\n";

        //M_gmodel->writeMSH( _name+"."+formatExportMesh, 2.2, CTX::instance()->mesh.binary );
        if ( outputMeshNamePath.extension() == ".msh" )
            M_gmodel->writeMSH( outputMeshName, 2.2, CTX::instance()->mesh.binary );
        else if ( outputMeshNamePath.extension() == ".stl" )
            M_gmodel->writeSTL( outputMeshName, CTX::instance()->mesh.binary );
        else
            CHECK( false ) << "error \n";

        delete M_gmodel;
    }


}

class ToolBoxBloodFlowReMeshSTL
{
public :

    ToolBoxBloodFlowReMeshSTL( std::string prefix )
        :
        M_prefix( prefix ),
        M_packageType(soption(_name="package-type",_prefix=this->prefix())),
        M_inputPath(soption(_name="input.filename",_prefix=this->prefix())),
        M_centerlinesFileName(soption(_name="centerlines.filename",_prefix=this->prefix())),
        M_remeshNbPointsInCircle( ioption(_name="nb-points-in-circle",_prefix=this->prefix()) ),
        M_area( doption(_name="area",_prefix=this->prefix()) ),
        M_outputDirectory( soption(_name="output.directory",_prefix=this->prefix()) ),
        M_forceRebuild( boption(_name="force-rebuild",_prefix=this->prefix() ) )
        {
            CHECK( M_packageType == "gmsh" || M_packageType == "vmtk" ) << "error on packageType : " << M_packageType;
            if ( !M_inputPath.empty() && M_outputPathGMSH.empty() )
            {
                this->updateOutputPathFromInputFileName();
            }
        }

    ToolBoxBloodFlowReMeshSTL( ToolBoxBloodFlowReMeshSTL const& e )
        :
        M_prefix( e.M_prefix ),
        M_packageType( e.M_packageType ),
        M_inputPath( e.M_inputPath ),
        M_centerlinesFileName( e.M_centerlinesFileName ),
        M_remeshNbPointsInCircle( e.M_remeshNbPointsInCircle ),
        M_area( e.M_area ),
        M_outputPathGMSH( e.M_outputPathGMSH ), M_outputPathVMTK( e.M_outputPathVMTK),
        M_outputDirectory( e.M_outputDirectory ),
        M_forceRebuild( e.M_forceRebuild )
        {}

    std::string prefix() const { return M_prefix; }
    WorldComm const& worldComm() const { return Environment::worldComm(); }
    std::string packageType() const { return M_packageType; }
    std::string inputPath() const { return M_inputPath; }
    std::string centerlinesFileName() const { return M_centerlinesFileName; }
    int remeshNbPointsInCircle() const { return M_remeshNbPointsInCircle; }
    double area() const { return M_area; }
    std::string outputPath() const { if ( this->packageType() =="gmsh" ) return M_outputPathGMSH; else return M_outputPathVMTK; }

    void setPackageType( std::string type)
    {
        CHECK( type == "gmsh" || type == "vmtk" ) << "error on packageType : " << type;
        M_packageType=type;
    }
    void setInputPath(std::string s) { M_inputPath=s; }
    void setCenterlinesFileName(std::string s) { M_centerlinesFileName=s; }

    bool forceRebuild() const { return M_forceRebuild; }


#if 0
    void updateOutputFileName()
    {
        std::string specStr;
        if ( this->packageType() == "gmsh" )
            specStr += (boost::format( "remeshGMSHpt%1%") %this->remeshNbPointsInCircle() ).str();
        else if ( this->packageType() == "vmtk" )
            specStr += (boost::format( "remeshVMTKarea%1%") %this->area() ).str();

        std::string outputName = (boost::format( "%1%_%2%.stl")
                                  %fs::path(this->inputPath()).stem().string()
                                  % specStr
                                  ).str();


        //std::string outputName = fs::path(this->inputPath()).stem().string()+"_remeshGMSH.stl";
        M_outputFileName = (fs::current_path()/outputName).string();
    }
#endif

    void updateOutputPathFromInputFileName()
    {
        CHECK( !M_inputPath.empty() ) << "input path is empty";

        // define output directory
        fs::path meshesdirectories = Environment::rootRepository();
        if ( M_outputDirectory.empty() )
        {
            //meshesdirectories /= fs::path("/angiotk/meshing/remesh-garbage/" + this->prefix() );
            meshesdirectories = fs::current_path();
        }
        else meshesdirectories /= fs::path(M_outputDirectory);

        // get filename without extension
        fs::path gp = M_inputPath;
        std::string nameMeshFile = gp.stem().string();


        std::string specGMSH = (boost::format( "remeshGMSHpt%1%") %this->remeshNbPointsInCircle() ).str();
        std::string specVMTK = (boost::format( "remeshVMTKarea%1%") %this->area() ).str();
        std::string newFileNameGMSH = (boost::format("%1%_%2%.stl")%nameMeshFile %specGMSH ).str();
        std::string newFileNameVMTK = (boost::format("%1%_%2%.stl")%nameMeshFile %specVMTK ).str();

        M_outputPathGMSH = ( meshesdirectories / fs::path(newFileNameGMSH) ).string();
        M_outputPathVMTK = ( meshesdirectories / fs::path(newFileNameVMTK) ).string();
    }
    void run()
    {
        std::cout << "\n"
                  << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n"
                  << "---------------------------------------\n"
                  << "run ToolBoxBloodFlowReMeshSTL \n"
                  << "---------------------------------------\n";
        std::cout << "type                 : " << M_packageType << "\n"
                  << "inputPath          : " << this->inputPath() << "\n";
        if ( this->packageType() == "gmsh" )
            std::cout << "centerlinesFileName  : " << this->centerlinesFileName() << "\n";
        else if ( this->packageType() == "vmtk" )
            std::cout << "area  : " << this->area() << "\n";
        std::cout << "output path       : " << this->outputPath() << "\n"
                  << "---------------------------------------\n"
                  << "---------------------------------------\n";

        if ( !this->forceRebuild() && fs::exists( this->outputPath() ) )
        {
            std::cout << "already file exist, ignore remeshSTL\n"
                      << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n";
            return;
        }


        fs::path directory;
        // build directories if necessary
        if ( !this->outputPath().empty() && this->worldComm().isMasterRank() )
        {
            directory = fs::path(this->outputPath()).parent_path();
            if ( !fs::exists( directory ) )
                fs::create_directories( directory );
        }
        // // wait all process
        this->worldComm().globalComm().barrier();


        if ( this->packageType() == "gmsh" )
            this->runGMSH();
        else if ( this->packageType() == "vmtk" )
            this->runVMTK();

        std::cout << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n\n";

    }
    void runVMTK()
    {
        CHECK( !this->inputPath().empty() ) << "inputPath is empty";



        std::ostringstream __str;
        // source ~/packages/vmtk/vmtk.build2/Install/vmtk_env.sh
        std::string pythonExecutable = BOOST_PP_STRINGIZE( PYTHON_EXECUTABLE );
        __str << pythonExecutable << " ";
        //std::string dirBaseVmtk = "/Users/vincentchabannes/packages/vmtk/vmtk.build2/Install/bin/";
        std::string dirBaseVmtk = BOOST_PP_STRINGIZE( VMTK_BINARY_DIR );
        __str << dirBaseVmtk << "/vmtk " << dirBaseVmtk << "/vmtksurfaceremeshing ";
        __str << "-ifile " << this->inputPath() << " ";
        __str << "-ofile " << this->outputPath() << " ";
        __str << "-area " << this->area() << " ";
        auto err = ::system( __str.str().c_str() );

        //std::cout << "hola\n"<< __str.str() <<"\n";
    }
    void runGMSH()
    {
        CHECK( !this->inputPath().empty() ) << "inputPath is empty";
        CHECK( !this->centerlinesFileName().empty() ) << "centerlinesFileName is empty";

        std::ostringstream geodesc;
        geodesc << "Mesh.Algorithm = 6; //(1=MeshAdapt, 5=Delaunay, 6=Frontal, 7=bamg, 8=delquad) \n"
                << "Mesh.Algorithm3D = 1; //(1=tetgen, 4=netgen, 7=MMG3D, 9=R-tree) \n";

        //geodesc << "Merge \"stl_remesh_vmtk/fluidskin3.stl\"\n";
        geodesc << "Merge \""<< this->inputPath() <<"\";\n";

        geodesc << "Field[1] = Centerline;\n";
        //geodesc << "Field[1].FileName = \"../centerlines/fluidskin3.vtk\"\n";
        geodesc << "Field[1].FileName = \"" << this->centerlinesFileName() << "\";\n";
        geodesc << "Field[1].nbPoints = "<< this->remeshNbPointsInCircle() << ";//15//25 //number of mesh elements in a circle\n";
        //geodesc << "Field[1].nbPoints = 15;//25 //number of mesh elements in a circle\n";

        //Remesh the initial stl
        geodesc << "Field[1].reMesh =1;\n"
                << "Field[1].run;\n"
                << "Background Field = 1;\n";

        fs::path outputMeshNamePath = fs::path(this->outputPath());
        std::string _name = outputMeshNamePath.stem().string();
        std::string geoname=_name+".geo";

        std::ofstream geofile( geoname.c_str() );
        geofile << geodesc.str();
        geofile.close();

        //std::cout << " this->outputFileName() " << this->outputFileName() << "\n";
        detail::generateMeshFromGeo(geoname/*remeshGeoFileName*/,this->outputPath()/*remeshMshFileName*/,2);
    }

    static
    po::options_description
    options( std::string const& prefix )
    {
        po::options_description myMeshSurfaceOptions( "Mesh surface blood flow from STL and Centerlines options" );
        myMeshSurfaceOptions.add_options()

            ( prefixvm(prefix,"package-type").c_str(), po::value<std::string>()->default_value( "vmtk" ), "force-remesh" )
            //( prefixvm(prefix,"force-remesh").c_str(), po::value<bool>()->default_value( false ), "force-remesh" )
            ( prefixvm(prefix,"nb-points-in-circle").c_str(), po::value<int>()->default_value( 15 ), "nb-points-in-circle" )
            ( prefixvm(prefix,"area").c_str(), po::value<double>()->default_value( 0.5 ), "area" )
            ( prefixvm(prefix,"input.filename").c_str(), po::value<std::string>()->default_value( "" ), "stl.filename" )
            ( prefixvm(prefix,"centerlines.filename").c_str(), po::value<std::string>()->default_value( "" ), "centerlines.filename" )

            (prefixvm(prefix,"output.directory").c_str(), Feel::po::value<std::string>()->default_value(""), "(string) output directory")
            (prefixvm(prefix,"force-rebuild").c_str(), Feel::po::value<bool>()->default_value(false), "(bool) force-rebuild")

            ;
        return myMeshSurfaceOptions;
    }

private :
    std::string M_prefix;
    std::string M_packageType;
    std::string M_inputPath;
    std::string M_centerlinesFileName;
    int M_remeshNbPointsInCircle;
    double M_area;
    std::string M_outputPathGMSH, M_outputPathVMTK;
    std::string M_outputDirectory;
    bool M_forceRebuild;

}; // class ToolBoxBloodFlowReMeshSTL

class ToolBoxBloodFlowMesh
{
public :

    ToolBoxBloodFlowMesh( std::string prefix )
        :
        M_prefix( prefix ),
        M_inputSTLPath(soption(_name="input.stl.filename",_prefix=this->prefix()) ),
        M_inputCenterlinesPath(soption(_name="input.centerlines.filename",_prefix=this->prefix())),
        M_inputInletOutletDescPath(soption(_name="input.inletoutlet-desc.filename",_prefix=this->prefix())),
        M_remeshNbPointsInCircle( ioption(_name="nb-points-in-circle",_prefix=this->prefix() ) ),
        M_extrudeWall( boption(_name="extrude-wall",_prefix=this->prefix() ) ),
        M_extrudeWallNbElemLayer( ioption(_name="extrude-wall.nb-elt-layer",_prefix=this->prefix() ) ),
        M_extrudeWallhLayer( doption(_name="extrude-wall.h-layer",_prefix=this->prefix()) ),
        M_outputPath(),
        M_outputDirectory( soption(_name="output.directory",_prefix=this->prefix()) ),
        M_forceRebuild( boption(_name="force-rebuild",_prefix=this->prefix() ) )
        {
            if ( !M_inputSTLPath.empty() && M_outputPath.empty() )
            {
                this->updateOutputPathFromInputFileName();
            }
        }
    ToolBoxBloodFlowMesh( ToolBoxBloodFlowMesh const& e )
        :
        M_prefix( e.M_prefix ),
        M_inputSTLPath( e.M_inputSTLPath ),
        M_inputCenterlinesPath( e.M_inputCenterlinesPath ),
        M_inputInletOutletDescPath( e.M_inputInletOutletDescPath ),
        M_remeshNbPointsInCircle( e.M_remeshNbPointsInCircle ),
        M_extrudeWall( e.M_extrudeWall ),
        M_extrudeWallNbElemLayer( e.M_extrudeWallNbElemLayer ),
        M_extrudeWallhLayer( e.M_extrudeWallhLayer ),
        M_outputPath( e.M_outputPath ),
        M_outputDirectory( e.M_outputDirectory ),
        M_forceRebuild( e.M_forceRebuild )
        {}

    std::string prefix() const { return M_prefix; }
    WorldComm const& worldComm() const { return Environment::worldComm(); }

    std::string inputSTLPath() const { return M_inputSTLPath; }
    std::string inputCenterlinesPath() const { return M_inputCenterlinesPath; }

    void setInputSTLPath(std::string s) { M_inputSTLPath=s; }
    void setInputCenterlinesPath(std::string s) { M_inputCenterlinesPath=s; }

    std::string inputInletOutletDescPath() const { return M_inputInletOutletDescPath; }
    void setInputInletOutletDescPath(std::string s) { M_inputInletOutletDescPath=s; }

    int remeshNbPointsInCircle() const { return M_remeshNbPointsInCircle; }

    bool extrudeWall() const { return M_extrudeWall; }
    int extrudeWall_nbElemLayer() const { return M_extrudeWallNbElemLayer; }
    double extrudeWall_hLayer() const { return M_extrudeWallhLayer; }

    std::string outputPath() const { return M_outputPath; }
#if 0
    void updateOutputFileName()
    {
        std::string arterialWallSpecStr = (this->extrudeWall())? "WithArterialWall" : "WithoutArterialWall";
        if ( this->extrudeWall() )
            arterialWallSpecStr += (boost::format("hLayer%1%nEltLayer%2%")%this->extrudeWall_hLayer() %this->extrudeWall_nbElemLayer() ).str();

        //std::string outputName = fs::path(this->inputSTLPath()).stem().string()+"_volumeMesh.vtk";
        std::string outputName = (boost::format( "%1%_%2%pt%3%%4%.msh")
                                  %fs::path(this->inputSTLPath()).stem().string()
                                  %"volumeMesh"
                                  %this->remeshNbPointsInCircle()
                                  %arterialWallSpecStr
                                  ).str();
        //std::string outputName = fs::path(this->inputSTLPath()).string()+"_centerlines.vtk";
        M_outputPath = (fs::current_path()/outputName).string();
    }
#endif
    void updateOutputPathFromInputFileName()
    {
        CHECK( !this->inputSTLPath().empty() ) << "input path is empty";

        // define output directory
        fs::path meshesdirectories = Environment::rootRepository();
        if ( M_outputDirectory.empty() )
        {
            //meshesdirectories /= fs::path("/angiotk/meshing/centerlines-garbage/" + this->prefix() );
            meshesdirectories = fs::current_path();
        }
        else meshesdirectories /= fs::path(M_outputDirectory);

        // get filename without extension
        fs::path gp = M_inputSTLPath;
        std::string nameMeshFile = gp.stem().string();



        std::string arterialWallSpecStr = (this->extrudeWall())? "WithArterialWall" : "WithoutArterialWall";
        if ( this->extrudeWall() )
            arterialWallSpecStr += (boost::format("hLayer%1%nEltLayer%2%")%this->extrudeWall_hLayer() %this->extrudeWall_nbElemLayer() ).str();

        std::string newFileName = (boost::format( "%1%_%2%pt%3%%4%.msh")
                                   %nameMeshFile
                                   %"volumeMesh"
                                   %this->remeshNbPointsInCircle()
                                   %arterialWallSpecStr
                                   ).str();


        fs::path outputPath = meshesdirectories / fs::path(newFileName);
        M_outputPath = outputPath.string();
    }

    void run()
    {
        std::cout << "\n"
                  << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n"
                  << "---------------------------------------\n"
                  << "run ToolBoxBloodFlowMesh \n"
                  << "---------------------------------------\n";
        std::cout << "input path             : " << this->inputSTLPath() << "\n"
                  << "centerlinesFileName     : " << this->inputCenterlinesPath() << "\n"
                  << "inletOutletDescFileName : " << this->inputInletOutletDescPath() << "\n"
                  << "output path          : " << this->outputPath() << "\n"
                  << "---------------------------------------\n"
                  << "---------------------------------------\n";

        CHECK( !this->inputSTLPath().empty() ) << "inputSTLPath is empty";
        CHECK( !this->inputCenterlinesPath().empty() ) << "centerlinesFileName is empty";
        CHECK( !this->inputInletOutletDescPath().empty() ) << "inletOutletDescFileName is empty";

        if ( fs::exists( this->outputPath() ) )
        {
            std::cout << "already file exist, ignore meshVolume\n"
                      << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n";
            return;
        }

        fs::path directory;
        // build directories if necessary
        if ( !this->outputPath().empty() && this->worldComm().isMasterRank() )
        {
            directory = fs::path(this->outputPath()).parent_path();
            if ( !fs::exists( directory ) )
                fs::create_directories( directory );
        }
        // // wait all process
        this->worldComm().globalComm().barrier();



#if 0
        if ( true )
        {
            std::string remeshGeoFileName = "firstStep.geo";
            std::string remeshMshFileName = "firstStep.stl";
            this->generateGeoForRemeshSurfaceFromSTLAndCenterlines(remeshGeoFileName);
            /*this->*/detail::generateMeshFromGeo(remeshGeoFileName,remeshMshFileName,2);

            this->setStlFileName( remeshMshFileName );
        }
        else
        {
            std::string remeshMshFileName = "firstStep.stl";
            this->setStlFileName( remeshMshFileName );
        }
#endif
        std::string volumeGeoFileName = "secondStep.geo";
        std::string volumeMshFileName = "secondStep.msh";
        this->generateGeoFor3dVolumeFromSTLAndCenterlines(volumeGeoFileName);
        detail::generateMeshFromGeo(volumeGeoFileName,this->outputPath()/*volumeMshFileName*/,3);

        std::cout << "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n";

    }
#if 0
    void generateMeshFromGeo( std::string inputGeoName,std::string outputMeshName,int dim/*,std::string formatExportMesh = "msh"*/)
    {
        //fs::path outputMeshNamePath=fs::path(Environment::findFile(outputMeshName));
        fs::path outputMeshNamePath=fs::path(outputMeshName);
        std::string _name = outputMeshNamePath.stem().string();
        std::cout << "\n _name " << _name << "\n";
        std::cout << "\n _ext " << outputMeshNamePath.extension() << "\n";

        GmshInitialize();

        GModel * M_gmodel = new GModel();
        M_gmodel->setName( _name );
#if !defined( __APPLE__ )
        M_gmodel->setFileName( _name );
#endif
        M_gmodel->readGEO( inputGeoName );
        M_gmodel->mesh( dim );

        CHECK(M_gmodel->getMeshStatus() > 0)  << "Invalid Gmsh Mesh, Gmsh status : " << M_gmodel->getMeshStatus() << " should be > 0. Gmsh mesh cannot be written to disk\n";

        //M_gmodel->writeMSH( _name+"."+formatExportMesh, 2.2, CTX::instance()->mesh.binary );
        if ( outputMeshNamePath.extension() == ".msh" )
            M_gmodel->writeMSH( outputMeshName, 2.2, CTX::instance()->mesh.binary );
        else if ( outputMeshNamePath.extension() == ".stl" )
            M_gmodel->writeSTL( outputMeshName, CTX::instance()->mesh.binary );
        else
            CHECK( false ) << "error \n";

        delete M_gmodel;
    }

#endif
#if 0
    void generateGeoForRemeshSurfaceFromSTLAndCenterlines(std::string geoname)
    {
        std::ostringstream geodesc;
        geodesc << "Mesh.Algorithm = 6; //(1=MeshAdapt, 5=Delaunay, 6=Frontal, 7=bamg, 8=delquad) \n"
                << "Mesh.Algorithm3D = 1; //(1=tetgen, 4=netgen, 7=MMG3D, 9=R-tree) \n";

        //geodesc << "Merge \"stl_remesh_vmtk/fluidskin3.stl\"\n";
        geodesc << "Merge \""<< this->inputSTLPath() <<"\";\n";

        geodesc << "Field[1] = Centerline;\n";
        //geodesc << "Field[1].FileName = \"../centerlines/fluidskin3.vtk\"\n";
        geodesc << "Field[1].FileName = \"" << this->inputCenterlinesPath() << "\";\n";
        geodesc << "Field[1].nbPoints = "<< this->remeshNbPointsInCircle() << ";//15//25 //number of mesh elements in a circle\n";
        //geodesc << "Field[1].nbPoints = 15;//25 //number of mesh elements in a circle\n";

        //Remesh the initial stl
        geodesc << "Field[1].reMesh =1;\n"
                << "Field[1].run;\n"
                << "Background Field = 1;\n";

        std::ofstream geofile( geoname.c_str() );
        geofile << geodesc.str();
        geofile.close();
    }
#endif
    void generateGeoFor3dVolumeFromSTLAndCenterlines(std::string geoname)
    {
        std::ostringstream geodesc;
        geodesc << "General.ExpertMode=1;\n";
        geodesc << "Mesh.Algorithm = 6; //(1=MeshAdapt, 5=Delaunay, 6=Frontal, 7=bamg, 8=delquad) \n"
                << "Mesh.Algorithm3D = 1; //(1=tetgen, 4=netgen, 7=MMG3D, 9=R-tree) \n";

        //Mesh.CharacteristicLengthFactor=0.015;

        //Merge "stl_remesh_gmsh/fluidskin_P15.stl";
        geodesc << "Merge \""<< this->inputSTLPath() <<"\";\n";

        geodesc << "Field[1] = Centerline;\n";
        // centerline file in vtk format
        geodesc << "Field[1].FileName = \"" << this->inputCenterlinesPath() << "\";\n";

        geodesc << "Field[1].nbPoints = "<< this->remeshNbPointsInCircle() << ";//15//25 //number of mesh elements in a circle\n";

        // close inlets and outlets with planar faces
        geodesc << "Field[1].closeVolume =1;\n";

        // extrude in the outward direction a vessel wall
        geodesc << "Field[1].extrudeWall ="<< ( this->extrudeWall() ? 1 : 0 ) <<";\n";
        if ( this->extrudeWall() )
        {
            geodesc << "Field[1].nbElemLayer = "<< this->extrudeWall_nbElemLayer() <<"; //number of layers\n";
            geodesc << "Field[1].hLayer = "<< this->extrudeWall_hLayer() <<";// extrusion thickness given as percent of vessel radius\n";
        }

        // identify inlets/outlets boundaries
        geodesc << "Field[1].descInletOutlet = \"" << this->inputInletOutletDescPath() << "\";\n";

        // apply field
        geodesc << "Field[1].run;\n"
                << "Background Field = 1;\n";


        std::ofstream geofile( geoname.c_str() );
        geofile << geodesc.str();
        geofile.close();
    }

    static
    po::options_description
    options( std::string const& prefix )
    {
        po::options_description myMeshVolumeOptions( "Mesh volume blood flow from STL and Centerlines options" );
        myMeshVolumeOptions.add_options()
            ( prefixvm(prefix,"input.stl.filename").c_str(), po::value<std::string>()->default_value( "" ), "stl.filename" )
            ( prefixvm(prefix,"input.centerlines.filename").c_str(), po::value<std::string>()->default_value( "" ), "centerlines.filename" )
            ( prefixvm(prefix,"input.inletoutlet-desc.filename").c_str(), po::value<std::string>()->default_value( "" ), "inletoutlet-desc.filename" )
            ( prefixvm(prefix,"output.directory").c_str(), Feel::po::value<std::string>()->default_value(""), "(string) output directory")
            ( prefixvm(prefix,"force-rebuild").c_str(), Feel::po::value<bool>()->default_value(false), "(bool) force-rebuild")
            ( prefixvm(prefix,"nb-points-in-circle").c_str(), po::value<int>()->default_value( 15 ), "nb-points-in-circle" )
            ( prefixvm(prefix,"extrude-wall").c_str(),po::value<bool>()->default_value( true ), "extrude-wall" )
            ( prefixvm(prefix,"extrude-wall.nb-elt-layer").c_str(), po::value<int>()->default_value( 2 ), "nb-elt-layer" )
            ( prefixvm(prefix,"extrude-wall.h-layer").c_str(), po::value<double>()->default_value( 0.2 ), "h-layer" )
            ;
        return myMeshVolumeOptions;
    }


private :

    std::string M_prefix;
    std::string M_inputSTLPath;
    std::string M_inputCenterlinesPath;

    std::string M_inputInletOutletDescPath;

    int M_remeshNbPointsInCircle;

    bool M_extrudeWall;
    int M_extrudeWallNbElemLayer;
    double M_extrudeWallhLayer;

    std::string M_outputPath;
    std::string M_outputDirectory;
    bool M_forceRebuild;

};

} // namespace Feel

#endif // __toolboxbloodflowmesh_H
