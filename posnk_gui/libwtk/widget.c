#include <cairo.h>
#include <clara/cinput.h>
#include <wtk/widget.h>
#include <stdlib.h>
#include <string.h>

void wtk_widget_do_clip(wtk_widget_t *widget, cairo_t *context)
{
	cllist_t *_w;
	cairo_matrix_t m;
	
	cairo_get_matrix(context, &m);
	cairo_translate(context, widget->rect.x, widget->rect.y);

	if (widget->flags & WTK_WIDGET_FLAG_DIRTY)
		cairo_rectangle(context, 0, 0, widget->rect.w, widget->rect.h);

	for (_w = widget->children.next; _w != &(widget->children); _w = _w->next)
		wtk_widget_do_clip((wtk_widget_t *) _w, context);

	cairo_set_matrix(context, &m);
}

void wtk_widget_render(wtk_widget_t *widget, cairo_t *context)
{
	cllist_t *_w;
	cairo_matrix_t m;
	
	cairo_get_matrix(context, &m);
	cairo_translate(context, widget->rect.x, widget->rect.y);

	if (widget->callbacks.paint)
		widget->callbacks.paint(widget, context);
	else {
		cairo_rectangle(context, 0, 0, widget->rect.w, widget->rect.h);
		cairo_set_source_rgb(context, 0.6, 0.6, 0.6);
		cairo_fill(context);
	}

	for (_w = widget->children.next; _w != &(widget->children); _w = _w->next)
		wtk_widget_render((wtk_widget_t *) _w, context);

	cairo_set_matrix(context, &m);
}

void wtk_widget_dispatch_event(wtk_widget_t *widget, clara_event_msg_t *event)
{
	switch (event->event_type) {
		case CLARA_EVENT_TYPE_KEY_DOWN:
			if (widget->callbacks.key_down)
				widget->callbacks.key_down(widget, event->param[0], 'X');
			break;

		case CLARA_EVENT_TYPE_KEY_UP:
			if (widget->callbacks.key_up)
				widget->callbacks.key_up(widget, event->param[0], 'X');
			break;

		case CLARA_EVENT_TYPE_MOUSE_BTN_DOWN:
			if (widget->callbacks.mouse_down)
				widget->callbacks.mouse_down(widget, event->ptr, event->param[0]);
			break;

		case CLARA_EVENT_TYPE_MOUSE_BTN_UP:
			if (widget->callbacks.mouse_up)
				widget->callbacks.mouse_up(widget, event->ptr, event->param[0]);
			break;

		case CLARA_EVENT_TYPE_MOUSE_MOVE:
			if (widget->callbacks.mouse_move)
				widget->callbacks.mouse_move(widget, event->ptr);
			break;
	}
}

void wtk_widget_handle_event(wtk_widget_t *widget, clara_event_msg_t *event)
{
	cllist_t *_w;
	wtk_widget_t *w;
	event->ptr.x -= widget->rect.x;
	event->ptr.y -= widget->rect.y;

	if (event->flags & CLARA_EVENT_FLAG_FOCUS) {
		if (widget->focused)
			wtk_widget_handle_event(widget->focused, event);
		else
			wtk_widget_dispatch_event(widget, event);
	} else {

		for (_w = widget->children.next; _w != &(widget->children); _w = _w->next) {
			w = (wtk_widget_t *) _w;
			if (clara_rect_test(w->rect, event->ptr)) {
				if (event->flags & CLARA_EVENT_FLAG_SETFOCUS)
					widget->focused = w;
				wtk_widget_handle_event(w, event);
				return;
			}
		}
		if (event->flags & CLARA_EVENT_FLAG_SETFOCUS)
			widget->focused = NULL;

		wtk_widget_dispatch_event(widget, event);
		
	}
	
}

void wtk_widget_resize(wtk_widget_t *widget, int width, int height)
{
	widget->rect.w = width;
	widget->rect.h = height;

	if (widget->callbacks.resize)
		widget->callbacks.resize(widget);

}

void wtk_widget_redraw(wtk_widget_t *widget)
{
	widget->flags |= WTK_WIDGET_FLAG_DIRTY;

}

void wtk_widget_add(wtk_widget_t *parent, wtk_widget_t *widget)
{
	widget->flags |= WTK_WIDGET_FLAG_DIRTY;
	cllist_add_end(&(parent->children), (cllist_t *) widget);

}

wtk_widget_t *wtk_create_widget(clara_rect_t rect)
{
	wtk_widget_t *w = malloc( sizeof( wtk_widget_t ) );
	if (!w)
		return NULL;
	memset( w, 0, sizeof( wtk_widget_t ) );
	cllist_create(&(w->children));
	w->flags = WTK_WIDGET_FLAG_DIRTY;
	w->rect = rect;	
	return w;
}