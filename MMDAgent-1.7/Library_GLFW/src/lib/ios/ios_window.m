//========================================================================
// GLFW - An OpenGL framework
// Platform:    Cocoa/NSOpenGL
// API Version: 2.7
// WWW:         http://www.glfw.org/
//------------------------------------------------------------------------
// Copyright (c) 2009-2010 Camilla Berglund <elmindreda@elmindreda.org>
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================

#include "internal.h"
#include <AvailabilityMacros.h>

#import <UIKit/UIKIt.h>
#import <OpenGLES/ES2/glext.h>


//************************************************************************
//****               Platform implementation functions                ****
//************************************************************************

//========================================================================
// Here is where the window is created, and the OpenGL rendering context is
// created
//========================================================================

int  _glfwPlatformOpenWindow( int width, int height,
                              const _GLFWwndconfig *wndconfig,
                              const _GLFWfbconfig *fbconfig )
{
    return GL_TRUE;
}


//========================================================================
// Properly kill the window / video display
//========================================================================

void _glfwPlatformCloseWindow( void )
{
}


//========================================================================
// Set the window title
//========================================================================

void _glfwPlatformSetWindowTitle( const char *title )
{
}

#ifdef MMDAGENT
void _glfwPlatformEnableFullScreen()
{
}
void _glfwPlatformDisableFullScreen()
{
}
void _glfwPlatformGetRenderingSize(*width, *height)
{
}
#endif /* MMDAGENT */

//========================================================================
// Set the window size
//========================================================================

//TODO
void _glfwPlatformSetWindowSize( int width, int height )
{
}


//========================================================================
// Set the window position
//========================================================================

//TODO
void _glfwPlatformSetWindowPos( int x, int y )
{
}


//========================================================================
// Iconify the window
//========================================================================

void _glfwPlatformIconifyWindow( void )
{
}


//========================================================================
// Restore (un-iconify) the window
//========================================================================

void _glfwPlatformRestoreWindow( void )
{
}


//========================================================================
// Swap buffers
//========================================================================
void _glfwPlatformSwapBuffers( void )
{
   EAGLContext *context = [EAGLContext currentContext];
   [context presentRenderbuffer:GL_RENDERBUFFER];
}


//========================================================================
// Set double buffering swap interval
//========================================================================

void _glfwPlatformSwapInterval( int interval )
{
}


//========================================================================
// Write back window parameters into GLFW window structure
//========================================================================

void _glfwPlatformRefreshWindowParams( void )
{
}


//========================================================================
// Poll for new window and input events
//========================================================================

void _glfwPlatformPollEvents( void )
{
}


//========================================================================
// Wait for new window and input events
//========================================================================

void _glfwPlatformWaitEvents( void )
{
}


//========================================================================
// Hide mouse cursor (lock it)
//========================================================================

void _glfwPlatformHideMouseCursor( void )
{
}


//========================================================================
// Show mouse cursor (unlock it)
//========================================================================

void _glfwPlatformShowMouseCursor( void )
{
}


//========================================================================
// Set physical mouse cursor position
//========================================================================

void _glfwPlatformSetMouseCursorPos( int x, int y )
{
}

