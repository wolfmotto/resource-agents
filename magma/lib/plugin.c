/*
  Copyright Red Hat, Inc. 2002-2004

  The Magma Cluster API Library is free software; you can redistribute
  it and/or modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either version
  2.1 of the License, or (at your option) any later version.

  The Magma Cluster API Library is distributed in the hope that it will
  be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
 */
/** @file
 * Plugin loading routines & No-op functions.
 */
#include <magma.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifdef MDEBUG
#include <mallocdbg.h>
#endif


/*
 * Define our _U-versions of our shared storage 
 */
int
_U_clu_null(cluster_plugin_t __attribute__ ((unused)) *cpp)
{
	printf("Unimplemented NULL function called\n");
	return 0;
}


cluster_member_list_t *
_U_clu_member_list(cluster_plugin_t __attribute__ ((unused)) *cpp,
		    char __attribute__ ((unused)) *groupname)
{
	errno = ENOSYS;
	return NULL;
}


int
_U_clu_quorum_status(cluster_plugin_t __attribute__ ((unused)) *cpp,
		      char __attribute__ ((unused)) *groupname)
{
	errno = ENOSYS;
	return -ENOSYS;
}


int
_U_clu_get_event(cluster_plugin_t __attribute__ ((unused)) *cpp,
		  int __attribute__((unused)) fd)
{
	errno = ENOSYS;
	return -ENOSYS;
}


int
_U_clu_login(cluster_plugin_t __attribute__ ((unused)) *cpp,
	      int __attribute__ ((unused)) fd,
	      char __attribute__ ((unused)) *groupname)
{
	errno = ENOSYS;
	return -ENOSYS;
}


int
_U_clu_logout(cluster_plugin_t __attribute__ ((unused)) *cpp,
	       int __attribute__((unused)) fd)
{
	errno = ENOSYS;
	return -ENOSYS;
}


int
_U_clu_close(cluster_plugin_t __attribute__ ((unused)) *cpp,
	      int __attribute__((unused)) fd)
{
	errno = ENOSYS;
	return -ENOSYS;
}


int
_U_clu_fence(cluster_plugin_t __attribute__ ((unused)) *cpp,
	      cluster_member_t __attribute__((unused)) *node)
{
	errno = ENOSYS;
	return -ENOSYS;
}


int
_U_clu_lock(cluster_plugin_t __attribute__ ((unused)) *cpp,
	     char *__attribute__ ((unused)) resource,
	     int __attribute__ ((unused)) flags,
	     void **__attribute__ ((unused)) lockpp)
{
	errno = ENOSYS;
	return -ENOSYS;
}


int
_U_clu_unlock(cluster_plugin_t __attribute__ ((unused)) *cpp,
	     char *__attribute__ ((unused)) resource,
	     void *__attribute__ ((unused)) lockp)
{
	errno = ENOSYS;
	return -ENOSYS;
}


char *
_U_clu_plugin_version(cluster_plugin_t __attribute__ ((unused)) *cpp)
{
	return "Unimplemented Version Function v1.0";
}


/**
 * Load a cluster plugin .so file and map all the functions
 * provided to entries in a cluster_plugin_t structure.
 *
 * @param libpath	Path to file.
 * @return		NULL on failure, or a newly allocated
 *			ClusterStorageDriver on success.
 */
cluster_plugin_t *
cp_load(const char *libpath)
{
	void *handle = NULL;
	cluster_plugin_t *cpp = NULL;
	double (*modversion)(void);

	handle = dlopen(libpath, RTLD_LAZY);
	if (!handle) {
		return NULL;
	}

	modversion = dlsym(handle, CLU_PLUGIN_VERSION_SYM);
	if (!modversion) {
		dlclose(handle);
		return NULL;
	}

	if (modversion() != CLUSTER_PLUGIN_API_VERSION) {
		fprintf(stderr, "API version mismatch in %s. %f expected; %f"
			" received.\n", libpath, CLUSTER_PLUGIN_API_VERSION,
			modversion());
		dlclose(handle);
		return NULL;
	}

	cpp = malloc(sizeof(*cpp));
	if (!cpp) {
		return NULL;
	}

	memset(cpp, 0, sizeof(*cpp));

	/* Initially, set everything to _U */
	CP_SET_UNIMP(cpp, null);
	CP_SET_UNIMP(cpp, member_list);
	CP_SET_UNIMP(cpp, login);
	CP_SET_UNIMP(cpp, logout);
	CP_SET_UNIMP(cpp, plugin_version);
	CP_SET_UNIMP(cpp, lock);
	CP_SET_UNIMP(cpp, unlock);
	
	/* Store the handle */
	cpp->cp_private.p_dlhandle = handle;

	/* Grab the init and deinit functions */
	cpp->cp_private.p_load_func = dlsym(handle, CLU_PLUGIN_LOAD_SYM);
	cpp->cp_private.p_init_func = dlsym(handle, CLU_PLUGIN_INIT_SYM);
	cpp->cp_private.p_unload_func = dlsym(handle, CLU_PLUGIN_UNLOAD_SYM);

	/*
	 * Modules *MUST* have a load function, and it can not fail.
	 */
	if (!cpp->cp_private.p_load_func) {
		fprintf(stderr, "Module load function not found in %s\n",
			libpath);
		free(cpp);
		dlclose(handle);
		return NULL;
	}

	/*
	 * Modules *MUST* have an init function.
	 */
	if (!cpp->cp_private.p_init_func) {
		fprintf(stderr, "Module init function not found in %s\n",
			libpath);
		free(cpp);
		dlclose(handle);
		return NULL;
	}

	if (cpp->cp_private.p_load_func(cpp) < 0) {
		free(cpp);
		return NULL;
	}

	return cpp;
}


/**
 * Initialize a cluster storage driver structure.  This calls the
 * initialization function we loaded in csd_load.
 *
 * @param cpp		Pointer to cluster storage driver to initialize.
 * @param priv		Optional driver-specific private data to copy in
 *			to cpp.
 * @param privlen	Size of data in priv.
 * @return		-1 on failure; 0 on success.
 */
int
cp_init(cluster_plugin_t *cpp, const void *priv, size_t privlen)
{
	/*
	 * Modules *MUST* have an initialization function, and it can not
	 * fail.
	 */
	if (!cpp->cp_private.p_init_func) {
		errno = ENOSYS;
		return -ENOSYS;
	}

	if ((cpp->cp_private.p_init_func)(cpp, priv, privlen) < 0) {
		return -EINVAL;
	}

	return 0;
}


int
cp_unload(cluster_plugin_t *cpp)
{
	void *handle;

	if (!cpp)
		return 0;

	/*
	 * Call the deinitialization function, if it exists.
	 */
	if (cpp->cp_private.p_unload_func &&
	    (cpp->cp_private.p_unload_func(cpp) < 0)) {
		return -EINVAL;
	}

	handle = cpp->cp_private.p_dlhandle;
	free(cpp);
	dlclose(handle);

	return 0;
}


/**
 * Use a specific cluster storage driver as the default for global "sh"
 * function calls.  This assigns the global function pointers to the
 * functions contained within the ClusterStorageDriver passed in.
 *
 * @param driver	Storage driver structure to make default.
 */
void
cp_set_default(cluster_plugin_t *driver)
{
	clu_set_default(driver);
}


/**
 * Clear out the default functions
 */
void
cp_reset(void)
{
	clu_clear_default();
}


int
cp_null(cluster_plugin_t *cpp)
{
	return cpp->cp_ops.s_null(cpp);
}


cluster_member_list_t *
cp_member_list(cluster_plugin_t *cpp, char *groupname)
{
	return cpp->cp_ops.s_member_list(cpp, groupname);
}


int
cp_quorum_status(cluster_plugin_t *cpp, char *groupname)
{
	return cpp->cp_ops.s_quorum_status(cpp, groupname);
}


char *
cp_plugin_version(cluster_plugin_t *cpp)
{
	return cpp->cp_ops.s_plugin_version(cpp);
}


int
cp_get_event(cluster_plugin_t *cpp, int fd)
{
	return cpp->cp_ops.s_get_event(cpp, fd);
}


int
cp_lock(cluster_plugin_t *cpp, char *resource, int flags, void **lockpp)
{
	return cpp->cp_ops.s_lock(cpp, resource, flags, lockpp);
}


int
cp_unlock(cluster_plugin_t *cpp, char *resource, void *lockp)
{
	return cpp->cp_ops.s_unlock(cpp, resource, lockp);
}


int
cp_login(cluster_plugin_t *cpp, int fd, char *groupname)
{
	return cpp->cp_ops.s_login(cpp, fd, groupname);
}


int
cp_open(cluster_plugin_t *cpp)
{
	return cpp->cp_ops.s_open(cpp);
}


int
cp_close(cluster_plugin_t *cpp, int fd)
{
	return cpp->cp_ops.s_close(cpp, fd);
}


int
cp_fence(cluster_plugin_t *cpp, cluster_member_t *node)
{
	return cpp->cp_ops.s_fence(cpp, node);
}


int
cp_logout(cluster_plugin_t *cpp, int fd)
{
	return cpp->cp_ops.s_logout(cpp, fd);
}


