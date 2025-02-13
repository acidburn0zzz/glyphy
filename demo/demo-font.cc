/*
 * Copyright 2012 Google, Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Google Author(s): Behdad Esfahbod
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "demo-font.h"

#include <glyphy-harfbuzz.h>

#include <map>
#include <vector>

typedef std::map<unsigned int, glyph_info_t> glyph_cache_t;

struct demo_font_t {
  unsigned int   refcount;

  hb_face_t     *face;
  hb_font_t     *font;
  glyph_cache_t *glyph_cache;
  demo_atlas_t  *atlas;
  glyphy_arc_accumulator_t *acc;

  /* stats */
  unsigned int num_glyphs;
  double       sum_error;
  unsigned int sum_endpoints;
  double       sum_fetch;
  unsigned int sum_bytes;
};

demo_font_t *
demo_font_create (hb_face_t    *face,
		  demo_atlas_t *atlas)
{
  demo_font_t *font = (demo_font_t *) calloc (1, sizeof (demo_font_t));
  font->refcount = 1;

  font->face = hb_face_reference (face);
  font->font = hb_font_create (face);
  font->glyph_cache = new glyph_cache_t ();
  font->atlas = demo_atlas_reference (atlas);
  font->acc = glyphy_arc_accumulator_create ();

  font->num_glyphs = 0;
  font->sum_error  = 0;
  font->sum_endpoints  = 0;
  font->sum_fetch  = 0;
  font->sum_bytes  = 0;

  return font;
}

demo_font_t *
demo_font_reference (demo_font_t *font)
{
  if (font) font->refcount++;
  return font;
}

void
demo_font_destroy (demo_font_t *font)
{
  if (!font || --font->refcount)
    return;

  glyphy_arc_accumulator_destroy (font->acc);
  demo_atlas_destroy (font->atlas);
  delete font->glyph_cache;
  hb_font_destroy (font->font);
  hb_face_destroy (font->face);
  free (font);
}


hb_face_t *
demo_font_get_face (demo_font_t *font)
{
  return font->face;
}

hb_font_t *
demo_font_get_font (demo_font_t *font)
{
  return font->font;
}

demo_atlas_t *
demo_font_get_atlas (demo_font_t *font)
{
  return font->atlas;
}


static glyphy_bool_t
accumulate_endpoint (glyphy_arc_endpoint_t         *endpoint,
		     std::vector<glyphy_arc_endpoint_t> *endpoints)
{
  endpoints->push_back (*endpoint);
  return true;
}

static void
encode_ft_glyph (demo_font_t      *font,
		 unsigned int      glyph_index,
		 double            tolerance_per_em,
		 glyphy_rgba_t    *buffer,
		 unsigned int      buffer_len,
		 unsigned int     *output_len,
		 unsigned int     *nominal_width,
		 unsigned int     *nominal_height,
		 glyphy_extents_t *extents,
		 double           *advance)
{
/* Used for testing only */
#define SCALE  (1. * (1 << 0))

  unsigned int upem = hb_face_get_upem (font->face);
  double tolerance = upem * tolerance_per_em; /* in font design units */
  double faraway = double (upem) / (MIN_FONT_SIZE * M_SQRT2);
  std::vector<glyphy_arc_endpoint_t> endpoints;

  glyphy_arc_accumulator_reset (font->acc);
  glyphy_arc_accumulator_set_tolerance (font->acc, tolerance);
  glyphy_arc_accumulator_set_callback (font->acc,
				       (glyphy_arc_endpoint_accumulator_callback_t) accumulate_endpoint,
				       &endpoints);

  glyphy_harfbuzz(font_get_glyph_shape) (font->font, glyph_index, font->acc);
  if (!glyphy_arc_accumulator_successful (font->acc))
    die ("Failed encoding arcs");

  assert (glyphy_arc_accumulator_get_error (font->acc) <= tolerance);

  if (endpoints.size ())
    glyphy_outline_winding_from_even_odd (&endpoints[0], endpoints.size (), false);

  if (SCALE != 1.)
    for (unsigned int i = 0; i < endpoints.size (); i++)
    {
      endpoints[i].p.x /= SCALE;
      endpoints[i].p.y /= SCALE;
    }

  double avg_fetch_achieved;
  if (!glyphy_arc_list_encode_blob (endpoints.size () ? &endpoints[0] : NULL, endpoints.size (),
				    buffer,
				    buffer_len,
				    faraway / SCALE,
				    4, /* UNUSED */
				    &avg_fetch_achieved,
				    output_len,
				    nominal_width,
				    nominal_height,
				    extents))
    die ("Failed encoding arcs");

  glyphy_extents_scale (extents, 1. / upem, 1. / upem);
  glyphy_extents_scale (extents, SCALE, SCALE);

  *advance = hb_font_get_glyph_h_advance (font->font, glyph_index) / (double) upem;

  if (0)
    LOGI ("gid%3u: endpoints%3d; err%3g%%; tex fetch%4.1f; mem%4.1fkb\n",
	  glyph_index,
	  (unsigned int) glyphy_arc_accumulator_get_num_endpoints (font->acc),
	  round (100 * glyphy_arc_accumulator_get_error (font->acc) / tolerance),
	  avg_fetch_achieved,
	  (*output_len * sizeof (glyphy_rgba_t)) / 1024.);

  font->num_glyphs++;
  font->sum_error += glyphy_arc_accumulator_get_error (font->acc) / tolerance;
  font->sum_endpoints += glyphy_arc_accumulator_get_num_endpoints (font->acc);
  font->sum_fetch += avg_fetch_achieved;
  font->sum_bytes += (*output_len * sizeof (glyphy_rgba_t));
}

static void
_demo_font_upload_glyph (demo_font_t *font,
			 unsigned int glyph_index,
			 glyph_info_t *glyph_info)
{
  glyphy_rgba_t buffer[4096 * 16];
  unsigned int output_len;

  encode_ft_glyph (font,
		   glyph_index,
		   TOLERANCE,
		   buffer, ARRAY_LEN (buffer),
		   &output_len,
		   &glyph_info->nominal_w,
		   &glyph_info->nominal_h,
		   &glyph_info->extents,
		   &glyph_info->advance);

  glyph_info->is_empty = glyphy_extents_is_empty (&glyph_info->extents);
  if (!glyph_info->is_empty)
    demo_atlas_alloc (font->atlas, buffer, output_len,
		      &glyph_info->atlas_x, &glyph_info->atlas_y);
}

void
demo_font_lookup_glyph (demo_font_t  *font,
			unsigned int  glyph_index,
			glyph_info_t *glyph_info)
{
  if (font->glyph_cache->find (glyph_index) == font->glyph_cache->end ()) {
    _demo_font_upload_glyph (font, glyph_index, glyph_info);
    (*font->glyph_cache)[glyph_index] = *glyph_info;
  } else
    *glyph_info = (*font->glyph_cache)[glyph_index];
}

void
demo_font_print_stats (demo_font_t *font)
{
  LOGI ("%3d glyphs; avg num endpoints%6.2f; avg error%5.1f%%; avg tex fetch%5.2f; avg %5.2fkb per glyph\n",
	font->num_glyphs,
	(double) font->sum_endpoints / font->num_glyphs,
	100. * font->sum_error / font->num_glyphs,
	font->sum_fetch / font->num_glyphs,
	font->sum_bytes / 1024. / font->num_glyphs);
}
