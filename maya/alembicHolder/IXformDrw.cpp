//-*****************************************************************************
//
// Copyright (c) 2009-2010,
//  Sony Pictures Imageworks, Inc. and
//  Industrial Light & Magic, a division of Lucasfilm Entertainment Company Ltd.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// *       Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// *       Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
// *       Neither the name of Sony Pictures Imageworks, nor
// Industrial Light & Magic nor the names of their contributors may be used
// to endorse or promote products derived from this software without specific
// prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//-*****************************************************************************

#include "IXformDrw.h"
#include "json/json.h"

namespace AlembicHolder {

//-*****************************************************************************
IXformDrw::IXformDrw( IXform &iXform, std::vector<std::string> path )
  : IObjectDrw( iXform, false, path )
  , m_xform( iXform )
{
    if ( !m_xform.valid() )
    {
        return;
    }

    m_localToParent.makeIdentity();



    // The object has already set up the min time and max time of
    // all the children.
    // if we have a non-constant time sampling, we should get times
    // out of it.
    const TimeSamplingPtr iTsmp = m_xform.getSchema().getTimeSampling();
    if ( !m_xform.getSchema().isConstant() )
    {
        size_t numSamps = m_xform.getSchema().getNumSamples();
        if ( numSamps > 0 )
        {
            chrono_t minTime = iTsmp->getSampleTime( 0 );
            m_minTime = std::min( m_minTime, minTime );
            chrono_t maxTime = iTsmp->getSampleTime( numSamps-1 );
            m_maxTime = std::max( m_maxTime, maxTime );
        }
    }

	m_currentFrame = MAnimControl::currentTime().value();
}

//-*****************************************************************************
IXformDrw::~IXformDrw()
{
    // Nothing
}

//-*****************************************************************************
bool IXformDrw::valid() const
{
    return IObjectDrw::valid() && m_xform.valid();
}


//-*****************************************************************************
void IXformDrw::setTime( chrono_t iSeconds )
{
	if ( !valid() )
	{
		m_localToParent.makeIdentity();
		return;
	}

	// Bail if time hasn't changed.
	if (iSeconds == m_currentFrame)
		return;

	// Use nearest to get our matrix.
	// Use nearest for now.
	ISampleSelector ss( iSeconds, ISampleSelector::kNearIndex );
	XformSample tmpXform;
	m_xform.getSchema().get(tmpXform,ss);
	m_localToParent = tmpXform.getMatrix();
	m_is_reflection = (getMMatrix().det3x3() < 0.0f);

	IObjectDrw::setTime( iSeconds );

	// Okay, now we need to recalculate the bounds.
	m_bounds.makeEmpty();
	for ( auto& dptr : m_children)
	{
		if (!dptr)
			continue;

		Box3d bnds = dptr->getBounds();
		if (bnds.isEmpty())
			continue;

		bnds = Imath::transform( bnds, m_localToParent );
		m_bounds.extendBy( bnds );
	}
}

Box3d IXformDrw::getBounds() const
{
    return m_bounds;
}

int IXformDrw::getNumTriangles() const
{
    if ( !valid() ) { return 0; }

    return IObjectDrw::getNumTriangles();

}

//-*****************************************************************************
void IXformDrw::draw( const DrawContext & iCtx )
{
    if ( !valid() ) { return; }

    static MGLFunctionTable *gGLFT = NULL;
    if (gGLFT == NULL)
       gGLFT = MHardwareRenderer::theRenderer()->glFunctionTable();

    M44d idenMatrix; // Defaults to identity.
    idenMatrix.makeIdentity();
    if ( m_localToParent.equalWithAbsError( idenMatrix, 1.0e-9 ) )
    {
        // Don't need to change anything.
        IObjectDrw::draw( iCtx );
        return;
    }

    gGLFT->glPushMatrix();
    // Get the current matrix.
    MGLdouble currentMatrix[16];
    gGLFT->glGetDoublev(     MGL_MODELVIEW_MATRIX, currentMatrix );

    // Basically, we want to load our matrix into the thingy.
    // We don't use the OpenGL transform stack because we have
    // deep deep hierarchy that exhausts the max stack depth quickly.
    gGLFT->glMatrixMode( MGL_MODELVIEW );
    gGLFT->glMultMatrixd( ( const MGLdouble * )&m_localToParent[0][0] );

    // Make a new context.
    M44d localToWorld = m_localToParent * iCtx.getLocalToWorld();
    DrawContext thisCtx( iCtx );
    thisCtx.setLocalToWorld( localToWorld );

    // Now draw.
    IObjectDrw::draw( thisCtx );

    // And back out, restore the matrix.
    gGLFT->glMatrixMode( MGL_MODELVIEW );
    gGLFT->glLoadMatrixd( currentMatrix );
    gGLFT->glPopMatrix();
}

} // End namespace AlembicHolder
