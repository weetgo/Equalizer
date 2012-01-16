
/* Copyright (c) 2011-2012, Stefan Eilemann <eile@eyescale.h> 
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 2.1 as published
 * by the Free Software Foundation.
 *  
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <fstream>

#include "server.h"

#include "display.h"
#include "resources.h"

#include "../config.h"
#include "../global.h"
#include "../loader.h"
#include "../server.h"

#include <fstream>

namespace eq
{
namespace server
{
namespace config
{

ServerPtr Server::configure( const std::string& session )
{
    Global::instance()->setConfigFAttribute( Config::FATTR_VERSION, 1.2f );
    ServerPtr server = new server::Server;

    Config* config = new Config( server );
    config->setName( session + " autoconfig" );

    if( !Resources::discover( config, session ))
        return 0;

    if( config->getNodes().size() > 1 )
        // add server connection for cluster configs
        server->addConnectionDescription( new ConnectionDescription );

    Display::discoverLocal( config );
    const Compounds compounds = Loader::addOutputCompounds( server );
    if( compounds.empty( ))
        return 0;

    const Channels channels = Resources::configureSourceChannels( config );
    Resources::configure( compounds, channels );

    if( session == "AffEnabled" ||  session == "WrongAffEnabled" )
    {
		const Nodes& nodes = config->getNodes();
		const int nbOfNodes = nodes.size();
		int32_t affEnabledCoreArr[3] = { 2, 3, 8 };
		int32_t wrongAffEnabledCoreArr[3] = { 8, 9, 2 };
		int32_t *affCoreArr = NULL;

		for(int i=0; i < nbOfNodes; i++)
		{
			const Pipes& pipes = nodes[i]->getPipes();
			const int nbOfPipes = pipes.size();
			EQINFO << "Number of pipes:" << nbOfPipes << std::endl;

			if( nbOfPipes == 3 )
			{
			    if( session == "AffEnabled" )
			        affCoreArr = affEnabledCoreArr;
			    else if( session == "WrongAffEnabled"  )
			        affCoreArr = wrongAffEnabledCoreArr;

			    pipes[0]->setIAttribute(Pipe::IATTR_HINT_AFFINITY, affCoreArr[0] );
                pipes[1]->setIAttribute(Pipe::IATTR_HINT_AFFINITY, affCoreArr[1] );
	            pipes[2]->setIAttribute(Pipe::IATTR_HINT_AFFINITY, affCoreArr[2] );
			}
		}
    }

    std::ofstream configFile;
    const std::string filename = session + ".auto.eqc";
    configFile.open( filename.c_str( ));
    configFile << co::base::indent << Global::instance() << *server
               << co::base::exdent << std::endl;
    configFile.close();

    return server;
}

}
}
}
