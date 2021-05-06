/*
  Copyright 2020-2021 David Robillard <d@drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

// Tests copy and paste within the same view

#undef NDEBUG

#include "test_utils.h"

#include "pugl/pugl.h"
#include "pugl/stub.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static const uintptr_t timerId = 1u;

typedef enum {
  START,
  EXPOSED,
  PASTED,
  RECEIVED_OFFER,
  RECEIVED_DATA,
  FINISHED,
} State;

typedef struct {
  PuglWorld*      world;
  PuglView*       view;
  PuglTestOptions opts;
  size_t          iteration;
  State           state;
} PuglTest;

static PuglStatus
onEvent(PuglView* view, const PuglEvent* event)
{
  PuglTest* test = (PuglTest*)puglGetHandle(view);

  if (test->opts.verbose) {
    printEvent(event, "Event: ", true);
  }

  switch (event->type) {
  case PUGL_EXPOSE:
    if (test->state < EXPOSED) {
      // Start timer on first expose
      assert(!puglStartTimer(view, timerId, 1 / 60.0));
      test->state = EXPOSED;
    }
    break;

  case PUGL_TIMER:
    assert(event->timer.id == timerId);

    if (test->iteration == 0) {
      puglSetClipboard(view,
                       PUGL_CLIPBOARD_GENERAL,
                       "text/plain",
                       "Copied Text",
                       strlen("Copied Text") + 1);

      // Check that the new type is available immediately
      assert(puglGetNumClipboardTypes(view, PUGL_CLIPBOARD_GENERAL) == 1);
      assert(!strcmp(puglGetClipboardType(view, PUGL_CLIPBOARD_GENERAL, 0),
                     "text/plain"));

      size_t      len = 0;
      const char* text =
        (const char*)puglGetClipboard(view, PUGL_CLIPBOARD_GENERAL, 0, &len);

      // Check that the new contents are available immediately
      assert(text);
      assert(!strcmp(text, "Copied Text"));

    } else if (test->iteration == 1) {
      size_t      len = 0;
      const char* text =
        (const char*)puglGetClipboard(view, PUGL_CLIPBOARD_GENERAL, 0, &len);

      // Check that the contents we pasted last iteration are still there
      assert(text);
      assert(!strcmp(text, "Copied Text"));

    } else if (test->iteration == 2) {
      // Start a "proper" paste
      assert(!puglPaste(view));
      test->state = PASTED;
    }

    ++test->iteration;
    break;

  case PUGL_DATA_OFFER:
    if (test->state == PASTED) {
      test->state = RECEIVED_OFFER;

      assert(!puglAcceptOffer(
        view, &event->offer, 0, PUGL_ACTION_COPY, puglGetFrame(view)));
    }
    break;

  case PUGL_DATA:
    if (test->state == RECEIVED_OFFER) {
      size_t      len = 0;
      const char* text =
        (const char*)puglGetClipboard(view, PUGL_CLIPBOARD_GENERAL, 0, &len);

      // Check that the offered data is what we copied earlier
      assert(text);
      assert(!strcmp(text, "Copied Text"));

      test->state = FINISHED;
    }
    break;

  default:
    break;
  }

  return PUGL_SUCCESS;
}

int
main(int argc, char** argv)
{
  PuglTest app = {puglNewWorld(PUGL_PROGRAM, 0),
                  NULL,
                  puglParseTestOptions(&argc, &argv),
                  0,
                  START};

  // Set up view
  app.view = puglNewView(app.world);
  puglSetClassName(app.world, "Pugl Test");
  puglSetBackend(app.view, puglStubBackend());
  puglSetHandle(app.view, &app);
  puglSetEventFunc(app.view, onEvent);
  puglSetDefaultSize(app.view, 512, 512);

  // Create and show window
  assert(!puglRealize(app.view));
  assert(!puglShow(app.view));

  // Run until the test is finished
  while (app.state != FINISHED) {
    assert(!puglUpdate(app.world, 1 / 15.0));
  }

  puglFreeView(app.view);
  puglFreeWorld(app.world);

  return 0;
}
