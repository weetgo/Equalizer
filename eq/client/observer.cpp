
/* Copyright (c) 2009-2013, Stefan Eilemann <eile@equalizergraphics.com>
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

#include "observer.h"

#include "config.h"
#include "client.h"
#include "event.h"
#include "eventICommand.h"
#include "server.h"

#include <eq/fabric/paths.h>
#include <eq/fabric/commands.h>
#include <co/bufferConnection.h>

#ifdef EQUALIZER_USE_OPENCV
#  include "detail/cvTracker.h"
#endif
#ifdef EQUALIZER_USE_VRPN
#  include <vrpn_Tracker.h>
#  include <co/buffer.h>
#else
   class vrpn_Tracker_Remote;
#endif


namespace eq
{
namespace detail
{
class CVTracker;

class Observer
{
public:
    Observer()
        : vrpnTracker( 0 )
        , cvTracker( 0 )
    {}

    vrpn_Tracker_Remote *vrpnTracker;
    CVTracker* cvTracker;
};
}

typedef fabric::Observer< Config, Observer > Super;

Observer::Observer( Config* parent )
        : Super( parent )
        , impl_( new detail::Observer )
{}

Observer::~Observer()
{
    delete impl_;
}

ServerPtr Observer::getServer()
{
    Config* config = getConfig();
    LBASSERT( config );
    return ( config ? config->getServer() : 0 );
}

#ifdef EQUALIZER_USE_VRPN
namespace
{
class MotionEvent
{
public:
    MotionEvent( const co::Object* object )
        : buffer( new co::BufferConnection )
        , command( co::Connections( 1, buffer ), fabric::CMD_CONFIG_EVENT,
                   co::COMMANDTYPE_OBJECT, object->getID(),
                   object->getInstanceID( ))
    {
        command << Event::OBSERVER_MOTION;
    }

    co::BufferConnectionPtr buffer;
    EventOCommand command;
};

void VRPN_CALLBACK trackerCB( void* userdata, const vrpn_TRACKERCB data )
{
    if( data.sensor != 0 )
        return; // Only use first sensor

    eq::Matrix4f head( eq::Matrix4f::IDENTITY );
    const vmml::quaternion<float> quat( data.quat[0], data.quat[2],
                                        -data.quat[1], data.quat[3] );
    quat.get_rotation_matrix( head );
    head.set_translation( data.pos[0], data.pos[2], -data.pos[1] );

    Observer *observer = static_cast< Observer* >( userdata );
    Config* config = observer->getConfig();

    MotionEvent oEvent( config );
    oEvent.command << observer->getID() << head;
    oEvent.command.disable();

    ClientPtr client = config->getClient();
    co::Buffer buffer;
    buffer.swap( oEvent.buffer->getBuffer( ));

    co::ICommand iCommand( client, client, &buffer, false );
    EventICommand iEvent( iCommand );
    iEvent.get< uint128_t >(); // normally done by Config::handleEvent()
    observer->handleEvent( iEvent );
}
}
#endif

bool Observer::configInit()
{
#ifdef EQUALIZER_USE_VRPN
    const std::string& vrpnName = getVRPNTracker();
    if( !vrpnName.empty( ))
    {
        impl_->vrpnTracker = new vrpn_Tracker_Remote( vrpnName.c_str( ));
        if( impl_->vrpnTracker->register_change_handler(this, trackerCB) != -1 )
            return true;

        LBWARN << "VRPN tracker couldn't connect to device " << vrpnName
               << std::endl;
        delete impl_->vrpnTracker;
        impl_->vrpnTracker = 0;
        return false;
    }
#endif
#ifdef EQUALIZER_USE_OPENCV
    int32_t camera = getOpenCVCamera();
    if( camera == OFF )
        return true;
    if( camera == AUTO )
        camera = getPath().observerIndex;
    else
        --camera; // .eqc counts from 1, OpenCV from 0

    impl_->cvTracker = new detail::CVTracker( this, camera );
    if( impl_->cvTracker->isGood( ))
        return impl_->cvTracker->start();

    delete impl_->cvTracker;
    impl_->cvTracker = 0;
    return getOpenCVCamera() == AUTO; // not a failure for auto setting
#endif
    return true;
}

bool Observer::handleEvent( EventICommand& command )
{
    switch( command.getEventType( ))
    {
    case Event::OBSERVER_MOTION:
        return setHeadMatrix( command.get< Matrix4f >( ));
    }
    return false;
}

bool Observer::configExit()
{
#ifdef EQUALIZER_USE_VRPN
    if( impl_->vrpnTracker )
    {
        impl_->vrpnTracker->unregister_change_handler( this, trackerCB );
        delete impl_->vrpnTracker;
        impl_->vrpnTracker = 0;
    }
#endif
#ifdef EQUALIZER_USE_OPENCV
    delete impl_->cvTracker;
    impl_->cvTracker = 0;
#endif
    return true;
}

void Observer::frameStart( const uint32_t frameNumber )
{
#ifdef EQUALIZER_USE_VRPN
    if( impl_->vrpnTracker )
        impl_->vrpnTracker->mainloop();
#endif
}

}

#include "../fabric/observer.ipp"
template class eq::fabric::Observer< eq::Config, eq::Observer >;

/** @cond IGNORE */
template EQFABRIC_API std::ostream& eq::fabric::operator << ( std::ostream&,
                      const eq::fabric::Observer< eq::Config, eq::Observer >& );
/** @endcond */