/* Generated by wayland-scanner 1.23.0 */

#ifndef WLR_GAMMA_CONTROL_UNSTABLE_V1_CLIENT_PROTOCOL_H
#define WLR_GAMMA_CONTROL_UNSTABLE_V1_CLIENT_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include "wayland-client.h"

#ifdef  __cplusplus
extern "C" {
#endif

/**
 * @page page_wlr_gamma_control_unstable_v1 The wlr_gamma_control_unstable_v1 protocol
 * manage gamma tables of outputs
 *
 * @section page_desc_wlr_gamma_control_unstable_v1 Description
 *
 * This protocol allows a privileged client to set the gamma tables for
 * outputs.
 *
 * Warning! The protocol described in this file is experimental and
 * backward incompatible changes may be made. Backward compatible changes
 * may be added together with the corresponding interface version bump.
 * Backward incompatible changes are done by bumping the version number in
 * the protocol and interface names and resetting the interface version.
 * Once the protocol is to be declared stable, the 'z' prefix and the
 * version number in the protocol and interface names are removed and the
 * interface version number is reset.
 *
 * @section page_ifaces_wlr_gamma_control_unstable_v1 Interfaces
 * - @subpage page_iface_zwlr_gamma_control_manager_v1 - manager to create per-output gamma controls
 * - @subpage page_iface_zwlr_gamma_control_v1 - adjust gamma tables for an output
 * @section page_copyright_wlr_gamma_control_unstable_v1 Copyright
 * <pre>
 *
 * Copyright © 2015 Giulio camuffo
 * Copyright © 2018 Simon Ser
 *
 * Permission to use, copy, modify, distribute, and sell this
 * software and its documentation for any purpose is hereby granted
 * without fee, provided that the above copyright notice appear in
 * all copies and that both that copyright notice and this permission
 * notice appear in supporting documentation, and that the name of
 * the copyright holders not be used in advertising or publicity
 * pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
 * THIS SOFTWARE.
 * </pre>
 */
struct wl_output;
struct zwlr_gamma_control_manager_v1;
struct zwlr_gamma_control_v1;

#ifndef ZWLR_GAMMA_CONTROL_MANAGER_V1_INTERFACE
#define ZWLR_GAMMA_CONTROL_MANAGER_V1_INTERFACE
/**
 * @page page_iface_zwlr_gamma_control_manager_v1 zwlr_gamma_control_manager_v1
 * @section page_iface_zwlr_gamma_control_manager_v1_desc Description
 *
 * This interface is a manager that allows creating per-output gamma
 * controls.
 * @section page_iface_zwlr_gamma_control_manager_v1_api API
 * See @ref iface_zwlr_gamma_control_manager_v1.
 */
/**
 * @defgroup iface_zwlr_gamma_control_manager_v1 The zwlr_gamma_control_manager_v1 interface
 *
 * This interface is a manager that allows creating per-output gamma
 * controls.
 */
extern const struct wl_interface zwlr_gamma_control_manager_v1_interface;
#endif
#ifndef ZWLR_GAMMA_CONTROL_V1_INTERFACE
#define ZWLR_GAMMA_CONTROL_V1_INTERFACE
/**
 * @page page_iface_zwlr_gamma_control_v1 zwlr_gamma_control_v1
 * @section page_iface_zwlr_gamma_control_v1_desc Description
 *
 * This interface allows a client to adjust gamma tables for a particular
 * output.
 *
 * The client will receive the gamma size, and will then be able to set gamma
 * tables. At any time the compositor can send a failed event indicating that
 * this object is no longer valid.
 *
 * There can only be at most one gamma control object per output, which
 * has exclusive access to this particular output. When the gamma control
 * object is destroyed, the gamma table is restored to its original value.
 * @section page_iface_zwlr_gamma_control_v1_api API
 * See @ref iface_zwlr_gamma_control_v1.
 */
/**
 * @defgroup iface_zwlr_gamma_control_v1 The zwlr_gamma_control_v1 interface
 *
 * This interface allows a client to adjust gamma tables for a particular
 * output.
 *
 * The client will receive the gamma size, and will then be able to set gamma
 * tables. At any time the compositor can send a failed event indicating that
 * this object is no longer valid.
 *
 * There can only be at most one gamma control object per output, which
 * has exclusive access to this particular output. When the gamma control
 * object is destroyed, the gamma table is restored to its original value.
 */
extern const struct wl_interface zwlr_gamma_control_v1_interface;
#endif

#define ZWLR_GAMMA_CONTROL_MANAGER_V1_GET_GAMMA_CONTROL 0
#define ZWLR_GAMMA_CONTROL_MANAGER_V1_DESTROY 1


/**
 * @ingroup iface_zwlr_gamma_control_manager_v1
 */
#define ZWLR_GAMMA_CONTROL_MANAGER_V1_GET_GAMMA_CONTROL_SINCE_VERSION 1
/**
 * @ingroup iface_zwlr_gamma_control_manager_v1
 */
#define ZWLR_GAMMA_CONTROL_MANAGER_V1_DESTROY_SINCE_VERSION 1

/** @ingroup iface_zwlr_gamma_control_manager_v1 */
static inline void
zwlr_gamma_control_manager_v1_set_user_data(struct zwlr_gamma_control_manager_v1 *zwlr_gamma_control_manager_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zwlr_gamma_control_manager_v1, user_data);
}

/** @ingroup iface_zwlr_gamma_control_manager_v1 */
static inline void *
zwlr_gamma_control_manager_v1_get_user_data(struct zwlr_gamma_control_manager_v1 *zwlr_gamma_control_manager_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zwlr_gamma_control_manager_v1);
}

static inline uint32_t
zwlr_gamma_control_manager_v1_get_version(struct zwlr_gamma_control_manager_v1 *zwlr_gamma_control_manager_v1)
{
	return wl_proxy_get_version((struct wl_proxy *) zwlr_gamma_control_manager_v1);
}

/**
 * @ingroup iface_zwlr_gamma_control_manager_v1
 *
 * Create a gamma control that can be used to adjust gamma tables for the
 * provided output.
 */
static inline struct zwlr_gamma_control_v1 *
zwlr_gamma_control_manager_v1_get_gamma_control(struct zwlr_gamma_control_manager_v1 *zwlr_gamma_control_manager_v1, struct wl_output *output)
{
	struct wl_proxy *id;

	id = wl_proxy_marshal_flags((struct wl_proxy *) zwlr_gamma_control_manager_v1,
			 ZWLR_GAMMA_CONTROL_MANAGER_V1_GET_GAMMA_CONTROL, &zwlr_gamma_control_v1_interface, wl_proxy_get_version((struct wl_proxy *) zwlr_gamma_control_manager_v1), 0, NULL, output);

	return (struct zwlr_gamma_control_v1 *) id;
}

/**
 * @ingroup iface_zwlr_gamma_control_manager_v1
 *
 * All objects created by the manager will still remain valid, until their
 * appropriate destroy request has been called.
 */
static inline void
zwlr_gamma_control_manager_v1_destroy(struct zwlr_gamma_control_manager_v1 *zwlr_gamma_control_manager_v1)
{
	wl_proxy_marshal_flags((struct wl_proxy *) zwlr_gamma_control_manager_v1,
			 ZWLR_GAMMA_CONTROL_MANAGER_V1_DESTROY, NULL, wl_proxy_get_version((struct wl_proxy *) zwlr_gamma_control_manager_v1), WL_MARSHAL_FLAG_DESTROY);
}

#ifndef ZWLR_GAMMA_CONTROL_V1_ERROR_ENUM
#define ZWLR_GAMMA_CONTROL_V1_ERROR_ENUM
enum zwlr_gamma_control_v1_error {
	/**
	 * invalid gamma tables
	 */
	ZWLR_GAMMA_CONTROL_V1_ERROR_INVALID_GAMMA = 1,
};
#endif /* ZWLR_GAMMA_CONTROL_V1_ERROR_ENUM */

/**
 * @ingroup iface_zwlr_gamma_control_v1
 * @struct zwlr_gamma_control_v1_listener
 */
struct zwlr_gamma_control_v1_listener {
	/**
	 * size of gamma ramps
	 *
	 * Advertise the size of each gamma ramp.
	 *
	 * This event is sent immediately when the gamma control object is
	 * created.
	 * @param size number of elements in a ramp
	 */
	void (*gamma_size)(void *data,
			   struct zwlr_gamma_control_v1 *zwlr_gamma_control_v1,
			   uint32_t size);
	/**
	 * object no longer valid
	 *
	 * This event indicates that the gamma control is no longer
	 * valid. This can happen for a number of reasons, including: - The
	 * output doesn't support gamma tables - Setting the gamma tables
	 * failed - Another client already has exclusive gamma control for
	 * this output - The compositor has transferred gamma control to
	 * another client
	 *
	 * Upon receiving this event, the client should destroy this
	 * object.
	 */
	void (*failed)(void *data,
		       struct zwlr_gamma_control_v1 *zwlr_gamma_control_v1);
};

/**
 * @ingroup iface_zwlr_gamma_control_v1
 */
static inline int
zwlr_gamma_control_v1_add_listener(struct zwlr_gamma_control_v1 *zwlr_gamma_control_v1,
				   const struct zwlr_gamma_control_v1_listener *listener, void *data)
{
	return wl_proxy_add_listener((struct wl_proxy *) zwlr_gamma_control_v1,
				     (void (**)(void)) listener, data);
}

#define ZWLR_GAMMA_CONTROL_V1_SET_GAMMA 0
#define ZWLR_GAMMA_CONTROL_V1_DESTROY 1

/**
 * @ingroup iface_zwlr_gamma_control_v1
 */
#define ZWLR_GAMMA_CONTROL_V1_GAMMA_SIZE_SINCE_VERSION 1
/**
 * @ingroup iface_zwlr_gamma_control_v1
 */
#define ZWLR_GAMMA_CONTROL_V1_FAILED_SINCE_VERSION 1

/**
 * @ingroup iface_zwlr_gamma_control_v1
 */
#define ZWLR_GAMMA_CONTROL_V1_SET_GAMMA_SINCE_VERSION 1
/**
 * @ingroup iface_zwlr_gamma_control_v1
 */
#define ZWLR_GAMMA_CONTROL_V1_DESTROY_SINCE_VERSION 1

/** @ingroup iface_zwlr_gamma_control_v1 */
static inline void
zwlr_gamma_control_v1_set_user_data(struct zwlr_gamma_control_v1 *zwlr_gamma_control_v1, void *user_data)
{
	wl_proxy_set_user_data((struct wl_proxy *) zwlr_gamma_control_v1, user_data);
}

/** @ingroup iface_zwlr_gamma_control_v1 */
static inline void *
zwlr_gamma_control_v1_get_user_data(struct zwlr_gamma_control_v1 *zwlr_gamma_control_v1)
{
	return wl_proxy_get_user_data((struct wl_proxy *) zwlr_gamma_control_v1);
}

static inline uint32_t
zwlr_gamma_control_v1_get_version(struct zwlr_gamma_control_v1 *zwlr_gamma_control_v1)
{
	return wl_proxy_get_version((struct wl_proxy *) zwlr_gamma_control_v1);
}

/**
 * @ingroup iface_zwlr_gamma_control_v1
 *
 * Set the gamma table. The file descriptor can be memory-mapped to provide
 * the raw gamma table, which contains successive gamma ramps for the red,
 * green and blue channels. Each gamma ramp is an array of 16-byte unsigned
 * integers which has the same length as the gamma size.
 *
 * The file descriptor data must have the same length as three times the
 * gamma size.
 */
static inline void
zwlr_gamma_control_v1_set_gamma(struct zwlr_gamma_control_v1 *zwlr_gamma_control_v1, int32_t fd)
{
	wl_proxy_marshal_flags((struct wl_proxy *) zwlr_gamma_control_v1,
			 ZWLR_GAMMA_CONTROL_V1_SET_GAMMA, NULL, wl_proxy_get_version((struct wl_proxy *) zwlr_gamma_control_v1), 0, fd);
}

/**
 * @ingroup iface_zwlr_gamma_control_v1
 *
 * Destroys the gamma control object. If the object is still valid, this
 * restores the original gamma tables.
 */
static inline void
zwlr_gamma_control_v1_destroy(struct zwlr_gamma_control_v1 *zwlr_gamma_control_v1)
{
	wl_proxy_marshal_flags((struct wl_proxy *) zwlr_gamma_control_v1,
			 ZWLR_GAMMA_CONTROL_V1_DESTROY, NULL, wl_proxy_get_version((struct wl_proxy *) zwlr_gamma_control_v1), WL_MARSHAL_FLAG_DESTROY);
}

#ifdef  __cplusplus
}
#endif

#endif
