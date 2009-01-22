#include "ogl_extensions.h"
#include "ogl_debug.h"

#include <math.h>


glCaps_t ogl_caps;

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void GetOGLScreen_Info( glCaps_t * pcaps )
{

	memset(pcaps, 0, sizeof(glCaps_t));

	// get any pure OpenGL device caps

	pcaps->gl_version     = glGetString(GL_VERSION);
	pcaps->gl_vendor      = glGetString(GL_VENDOR);
	pcaps->gl_renderer    = glGetString(GL_RENDERER);
	pcaps->gl_extensions  = glGetString(GL_EXTENSIONS);

	pcaps->glu_version    = gluGetString(GL_VERSION);
	pcaps->glu_extensions = gluGetString(GL_EXTENSIONS);

	glGetIntegerv(GL_MAX_MODELVIEW_STACK_DEPTH,     &pcaps->max_modelview_stack_depth);
	glGetIntegerv(GL_MAX_PROJECTION_STACK_DEPTH,    &pcaps->max_projection_stack_depth);
	glGetIntegerv(GL_MAX_TEXTURE_STACK_DEPTH,       &pcaps->max_texture_stack_depth);
	glGetIntegerv(GL_MAX_NAME_STACK_DEPTH,          &pcaps->max_name_stack_depth);
	glGetIntegerv(GL_MAX_ATTRIB_STACK_DEPTH,        &pcaps->max_attrib_stack_depth);
	glGetIntegerv(GL_MAX_CLIENT_ATTRIB_STACK_DEPTH, &pcaps->max_client_attrib_stack_depth);

	glGetIntegerv(GL_SUBPIXEL_BITS,        &pcaps->subpixel_bits);
	glGetFloatv(GL_POINT_SIZE_RANGE,        pcaps->point_size_range);
	glGetFloatv(GL_POINT_SIZE_GRANULARITY, &pcaps->point_size_granularity);
	glGetFloatv(GL_LINE_WIDTH_RANGE,        pcaps->line_width_range);
	glGetFloatv(GL_LINE_WIDTH_GRANULARITY, &pcaps->line_width_granularity);

	glGetIntegerv(GL_MAX_VIEWPORT_DIMS, pcaps->max_viewport_dims);
	glGetBooleanv(GL_AUX_BUFFERS,      &pcaps->aux_buffers);
	glGetBooleanv(GL_RGBA_MODE,        &pcaps->rgba_mode);
	glGetBooleanv(GL_INDEX_MODE,       &pcaps->index_mode);
	glGetBooleanv(GL_DOUBLEBUFFER,     &pcaps->doublebuffer);
	glGetBooleanv(GL_STEREO,           &pcaps->stereo);
	glGetIntegerv(GL_RED_BITS,         &pcaps->red_bits);
	glGetIntegerv(GL_GREEN_BITS,       &pcaps->green_bits);
	glGetIntegerv(GL_BLUE_BITS,        &pcaps->blue_bits);
	glGetIntegerv(GL_ALPHA_BITS,       &pcaps->alpha_bits);
	glGetIntegerv(GL_INDEX_BITS,       &pcaps->index_bits);
	glGetIntegerv(GL_DEPTH_BITS,       &pcaps->depth_bits);
	glGetIntegerv(GL_STENCIL_BITS,     &pcaps->stencil_bits);
	glGetIntegerv(GL_ACCUM_RED_BITS,   &pcaps->accum_red_bits);
	glGetIntegerv(GL_ACCUM_GREEN_BITS, &pcaps->accum_green_bits);
	glGetIntegerv(GL_ACCUM_BLUE_BITS,  &pcaps->accum_blue_bits);
	glGetIntegerv(GL_ACCUM_ALPHA_BITS, &pcaps->accum_alpha_bits);

	glGetIntegerv(GL_MAX_LIGHTS,       &pcaps->max_lights);
	glGetIntegerv(GL_MAX_CLIP_PLANES,  &pcaps->max_clip_planes);
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &pcaps->max_texture_size);

	glGetIntegerv(GL_MAX_PIXEL_MAP_TABLE, &pcaps->max_pixel_map_table);
	glGetIntegerv(GL_MAX_LIST_NESTING,    &pcaps->max_list_nesting);
	glGetIntegerv(GL_MAX_EVAL_ORDER,      &pcaps->max_eval_order);

	pcaps->maxAnisotropy  = 0;
	pcaps->log2Anisotropy = 0;


	/// get the supported values for anisotropic filtering
	pcaps->anisotropic_supported = GL_FALSE;
	pcaps->maxAnisotropy  = 1.0f;
	pcaps->log2Anisotropy = 0.0f;
	if ( NULL != strstr((char*)pcaps->gl_extensions, "GL_EXT_texture_filter_anisotropic") )
	{
		pcaps->anisotropic_supported = GL_TRUE;
		glGetFloatv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &(pcaps->maxAnisotropy) );
		pcaps->log2Anisotropy = ( pcaps->maxAnisotropy == 0 ) ? 0 : floor( log( pcaps->maxAnisotropy + 1e-6 ) / log( 2.0f ) );
	}

}

//------------------------------------------------------------------------------
GLboolean ogl_video_parameters_default(ogl_video_parameters_t * pvid)
{
	if (NULL == pvid) return GL_FALSE;

	pvid->antialiasing = GL_FALSE;            ///< current antialiasing value
	pvid->perspective  = GL_FASTEST;          ///< current correction hint
	pvid->dither       = GL_FALSE;            ///< current dithering flag
	pvid->shading      = GL_SMOOTH;           ///< current shading type

	return GL_TRUE;
}

