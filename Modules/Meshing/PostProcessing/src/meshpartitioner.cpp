
#include <feel/feelcore/environment.hpp>

#include <postprocessing.hpp>


int main( int argc, char** argv )
{
    using namespace Feel;

    po::options_description myoptions = MeshPartitioner::options( "" );

    Environment env( _argc=argc, _argv=argv,
                     _desc=myoptions,
		     _about=about(_name="meshpartitioner",
				  _author="Feel++ Consortium",
				  _email="feelpp-devel@feelpp.org"));

    MeshPartitioner myPartitioner("");
    myPartitioner.run();

    return 0;
}
