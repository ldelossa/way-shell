/*
 * This file is generated by gdbus-codegen, do not modify it.
 *
 * The license of this code is the same as for the D-Bus interface description
 * it was derived from. Note that it links to GLib, so must comply with the
 * LGPL linking clauses.
 */

#ifndef __POWER_PROFILES_DBUS_H__
#define __POWER_PROFILES_DBUS_H__

#include <gio/gio.h>

G_BEGIN_DECLS


/* ------------------------------------------------------------------------ */
/* Declarations for net.hadess.PowerProfiles */

#define DBUS_TYPE_POWER_PROFILES (dbus_power_profiles_get_type ())
#define DBUS_POWER_PROFILES(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), DBUS_TYPE_POWER_PROFILES, DbusPowerProfiles))
#define DBUS_IS_POWER_PROFILES(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), DBUS_TYPE_POWER_PROFILES))
#define DBUS_POWER_PROFILES_GET_IFACE(o) (G_TYPE_INSTANCE_GET_INTERFACE ((o), DBUS_TYPE_POWER_PROFILES, DbusPowerProfilesIface))

struct _DbusPowerProfiles;
typedef struct _DbusPowerProfiles DbusPowerProfiles;
typedef struct _DbusPowerProfilesIface DbusPowerProfilesIface;

struct _DbusPowerProfilesIface
{
  GTypeInterface parent_iface;



  gboolean (*handle_hold_profile) (
    DbusPowerProfiles *object,
    GDBusMethodInvocation *invocation,
    const gchar *arg_profile,
    const gchar *arg_reason,
    const gchar *arg_application_id);

  gboolean (*handle_release_profile) (
    DbusPowerProfiles *object,
    GDBusMethodInvocation *invocation,
    guint arg_cookie);

  const gchar *const * (*get_actions) (DbusPowerProfiles *object);

  const gchar * (*get_active_profile) (DbusPowerProfiles *object);

  GVariant * (*get_active_profile_holds) (DbusPowerProfiles *object);

  const gchar * (*get_performance_degraded) (DbusPowerProfiles *object);

  const gchar * (*get_performance_inhibited) (DbusPowerProfiles *object);

  GVariant * (*get_profiles) (DbusPowerProfiles *object);

  void (*profile_released) (
    DbusPowerProfiles *object,
    guint arg_cookie);

};

GType dbus_power_profiles_get_type (void) G_GNUC_CONST;

GDBusInterfaceInfo *dbus_power_profiles_interface_info (void);
guint dbus_power_profiles_override_properties (GObjectClass *klass, guint property_id_begin);


/* D-Bus method call completion functions: */
void dbus_power_profiles_complete_hold_profile (
    DbusPowerProfiles *object,
    GDBusMethodInvocation *invocation,
    guint cookie);

void dbus_power_profiles_complete_release_profile (
    DbusPowerProfiles *object,
    GDBusMethodInvocation *invocation);



/* D-Bus signal emissions functions: */
void dbus_power_profiles_emit_profile_released (
    DbusPowerProfiles *object,
    guint arg_cookie);



/* D-Bus method calls: */
void dbus_power_profiles_call_hold_profile (
    DbusPowerProfiles *proxy,
    const gchar *arg_profile,
    const gchar *arg_reason,
    const gchar *arg_application_id,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean dbus_power_profiles_call_hold_profile_finish (
    DbusPowerProfiles *proxy,
    guint *out_cookie,
    GAsyncResult *res,
    GError **error);

gboolean dbus_power_profiles_call_hold_profile_sync (
    DbusPowerProfiles *proxy,
    const gchar *arg_profile,
    const gchar *arg_reason,
    const gchar *arg_application_id,
    guint *out_cookie,
    GCancellable *cancellable,
    GError **error);

void dbus_power_profiles_call_release_profile (
    DbusPowerProfiles *proxy,
    guint arg_cookie,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data);

gboolean dbus_power_profiles_call_release_profile_finish (
    DbusPowerProfiles *proxy,
    GAsyncResult *res,
    GError **error);

gboolean dbus_power_profiles_call_release_profile_sync (
    DbusPowerProfiles *proxy,
    guint arg_cookie,
    GCancellable *cancellable,
    GError **error);



/* D-Bus property accessors: */
const gchar *dbus_power_profiles_get_active_profile (DbusPowerProfiles *object);
gchar *dbus_power_profiles_dup_active_profile (DbusPowerProfiles *object);
void dbus_power_profiles_set_active_profile (DbusPowerProfiles *object, const gchar *value);

const gchar *dbus_power_profiles_get_performance_inhibited (DbusPowerProfiles *object);
gchar *dbus_power_profiles_dup_performance_inhibited (DbusPowerProfiles *object);
void dbus_power_profiles_set_performance_inhibited (DbusPowerProfiles *object, const gchar *value);

const gchar *dbus_power_profiles_get_performance_degraded (DbusPowerProfiles *object);
gchar *dbus_power_profiles_dup_performance_degraded (DbusPowerProfiles *object);
void dbus_power_profiles_set_performance_degraded (DbusPowerProfiles *object, const gchar *value);

GVariant *dbus_power_profiles_get_profiles (DbusPowerProfiles *object);
GVariant *dbus_power_profiles_dup_profiles (DbusPowerProfiles *object);
void dbus_power_profiles_set_profiles (DbusPowerProfiles *object, GVariant *value);

const gchar *const *dbus_power_profiles_get_actions (DbusPowerProfiles *object);
gchar **dbus_power_profiles_dup_actions (DbusPowerProfiles *object);
void dbus_power_profiles_set_actions (DbusPowerProfiles *object, const gchar *const *value);

GVariant *dbus_power_profiles_get_active_profile_holds (DbusPowerProfiles *object);
GVariant *dbus_power_profiles_dup_active_profile_holds (DbusPowerProfiles *object);
void dbus_power_profiles_set_active_profile_holds (DbusPowerProfiles *object, GVariant *value);


/* ---- */

#define DBUS_TYPE_POWER_PROFILES_PROXY (dbus_power_profiles_proxy_get_type ())
#define DBUS_POWER_PROFILES_PROXY(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), DBUS_TYPE_POWER_PROFILES_PROXY, DbusPowerProfilesProxy))
#define DBUS_POWER_PROFILES_PROXY_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), DBUS_TYPE_POWER_PROFILES_PROXY, DbusPowerProfilesProxyClass))
#define DBUS_POWER_PROFILES_PROXY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), DBUS_TYPE_POWER_PROFILES_PROXY, DbusPowerProfilesProxyClass))
#define DBUS_IS_POWER_PROFILES_PROXY(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), DBUS_TYPE_POWER_PROFILES_PROXY))
#define DBUS_IS_POWER_PROFILES_PROXY_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), DBUS_TYPE_POWER_PROFILES_PROXY))

typedef struct _DbusPowerProfilesProxy DbusPowerProfilesProxy;
typedef struct _DbusPowerProfilesProxyClass DbusPowerProfilesProxyClass;
typedef struct _DbusPowerProfilesProxyPrivate DbusPowerProfilesProxyPrivate;

struct _DbusPowerProfilesProxy
{
  /*< private >*/
  GDBusProxy parent_instance;
  DbusPowerProfilesProxyPrivate *priv;
};

struct _DbusPowerProfilesProxyClass
{
  GDBusProxyClass parent_class;
};

GType dbus_power_profiles_proxy_get_type (void) G_GNUC_CONST;

#if GLIB_CHECK_VERSION(2, 44, 0)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (DbusPowerProfilesProxy, g_object_unref)
#endif

void dbus_power_profiles_proxy_new (
    GDBusConnection     *connection,
    GDBusProxyFlags      flags,
    const gchar         *name,
    const gchar         *object_path,
    GCancellable        *cancellable,
    GAsyncReadyCallback  callback,
    gpointer             user_data);
DbusPowerProfiles *dbus_power_profiles_proxy_new_finish (
    GAsyncResult        *res,
    GError             **error);
DbusPowerProfiles *dbus_power_profiles_proxy_new_sync (
    GDBusConnection     *connection,
    GDBusProxyFlags      flags,
    const gchar         *name,
    const gchar         *object_path,
    GCancellable        *cancellable,
    GError             **error);

void dbus_power_profiles_proxy_new_for_bus (
    GBusType             bus_type,
    GDBusProxyFlags      flags,
    const gchar         *name,
    const gchar         *object_path,
    GCancellable        *cancellable,
    GAsyncReadyCallback  callback,
    gpointer             user_data);
DbusPowerProfiles *dbus_power_profiles_proxy_new_for_bus_finish (
    GAsyncResult        *res,
    GError             **error);
DbusPowerProfiles *dbus_power_profiles_proxy_new_for_bus_sync (
    GBusType             bus_type,
    GDBusProxyFlags      flags,
    const gchar         *name,
    const gchar         *object_path,
    GCancellable        *cancellable,
    GError             **error);


/* ---- */

#define DBUS_TYPE_POWER_PROFILES_SKELETON (dbus_power_profiles_skeleton_get_type ())
#define DBUS_POWER_PROFILES_SKELETON(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), DBUS_TYPE_POWER_PROFILES_SKELETON, DbusPowerProfilesSkeleton))
#define DBUS_POWER_PROFILES_SKELETON_CLASS(k) (G_TYPE_CHECK_CLASS_CAST ((k), DBUS_TYPE_POWER_PROFILES_SKELETON, DbusPowerProfilesSkeletonClass))
#define DBUS_POWER_PROFILES_SKELETON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), DBUS_TYPE_POWER_PROFILES_SKELETON, DbusPowerProfilesSkeletonClass))
#define DBUS_IS_POWER_PROFILES_SKELETON(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), DBUS_TYPE_POWER_PROFILES_SKELETON))
#define DBUS_IS_POWER_PROFILES_SKELETON_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), DBUS_TYPE_POWER_PROFILES_SKELETON))

typedef struct _DbusPowerProfilesSkeleton DbusPowerProfilesSkeleton;
typedef struct _DbusPowerProfilesSkeletonClass DbusPowerProfilesSkeletonClass;
typedef struct _DbusPowerProfilesSkeletonPrivate DbusPowerProfilesSkeletonPrivate;

struct _DbusPowerProfilesSkeleton
{
  /*< private >*/
  GDBusInterfaceSkeleton parent_instance;
  DbusPowerProfilesSkeletonPrivate *priv;
};

struct _DbusPowerProfilesSkeletonClass
{
  GDBusInterfaceSkeletonClass parent_class;
};

GType dbus_power_profiles_skeleton_get_type (void) G_GNUC_CONST;

#if GLIB_CHECK_VERSION(2, 44, 0)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (DbusPowerProfilesSkeleton, g_object_unref)
#endif

DbusPowerProfiles *dbus_power_profiles_skeleton_new (void);


G_END_DECLS

#endif /* __POWER_PROFILES_DBUS_H__ */
