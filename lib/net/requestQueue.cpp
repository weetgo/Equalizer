
/* Copyright (c) 2005-2006, Stefan Eilemann <eile@equalizergraphics.com> 
   All rights reserved. */

#include "requestQueue.h"

#include "packets.h"

using namespace eqNet;
using namespace std;

RequestQueue::RequestQueue()
        : _lastRequest(NULL)
{
    CHECK_THREAD_INIT( _threadID );
}

RequestQueue::~RequestQueue()
{
    if( _lastRequest )
        _requestCache.release( _lastRequest );
}

void RequestQueue::push( Node* node, const Packet* packet )
{
    _requestCacheLock.set();
    Request* request = _requestCache.alloc( node, packet );
    _requestCacheLock.unset();

    request->packet->command++; // REQ must always follow CMD
    _requests.push( request );
}

void RequestQueue::pushFront( Node* node, const Packet* packet )
{
    _requestCacheLock.set();
    Request* request = _requestCache.alloc( node, packet );
    _requestCacheLock.unset();

    request->packet->command++; // REQ must always follow CMD
    _requests.pushFront( request );
}

void RequestQueue::pop( Node** node, Packet** packet )
{
    CHECK_THREAD( _threadID );

    if( _lastRequest )
    {
        _requestCacheLock.set();
        _requestCache.release( _lastRequest );
        _requestCacheLock.unset();
    }
    
    _lastRequest = _requests.pop();
    if( node )   *node   = _lastRequest->node;
    if( packet ) *packet = _lastRequest->packet;
}

bool RequestQueue::tryPop( Node** node, Packet** packet )
{
    CHECK_THREAD( _threadID );

    Request* request = _requests.tryPop();
    if( !request )
        return false;

    if( _lastRequest )
    {
        _requestCacheLock.set();
        _requestCache.release( _lastRequest );
        _requestCacheLock.unset();
    }
    
    _lastRequest = request;
    if( node )   *node   = _lastRequest->node;
    if( packet ) *packet = _lastRequest->packet;
    return true;
}

bool RequestQueue::back( Node** node, Packet** packet ) const
{
    const Request* request = _requests.back();
    if( !request )
        return false;

    *node        = request->node;
    *packet      = request->packet;
    return true;
}
