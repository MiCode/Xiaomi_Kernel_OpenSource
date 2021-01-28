/*************************************************************************/ /*!
@File
@Title          Functions for creating debugfs directories and entries.
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include <linux/module.h>
#include <linux/slab.h>

#include <pvr_debugfs.h>
#include <hash.h>
#include "allocmem.h"
#include "pvr_bridge_k.h"

#define PVR_DEBUGFS_PVR_DPF_LEVEL PVR_DBG_ERROR

#define PVR_DEBUGFS_DIR_NAME PVR_DRM_NAME

/* Maximum number of debugfs files present at a time */
#define NUM_HASH_ENTRIES 250

/* Lock used when :
 * 1) adjusting refCounts
 * 2) deleting entries
 * 3) inserting, retrieving and removing entries from gHashTable */
static struct mutex gDebugFSHashAndRefLock;

/* Hash table to store pointers to allocated memories.
   It is supposed to help avoiding use-after-free cases */
static HASH_TABLE *gHashTable;

typedef struct _PVR_DEBUGFS_DIR_
{
	struct dentry*        psDirEntry;
	PPVR_DEBUGFS_DIR_DATA psParentDir;
	IMG_UINT32            ui32DirRefCount;
} PVR_DEBUGFS_DIR;

typedef struct _PVR_DEBUGFS_FILE_
{
	struct dentry*               psFileEntry;
	PVR_DEBUGFS_DIR*             psParentDir;
	const struct seq_operations* psReadOps;
	OS_STATS_PRINT_FUNC*         pfnStatsPrint;
	PVRSRV_ENTRY_WRITE_FUNC*     pfnWrite;
	IMG_UINT32                   ui32FileRefCount;
	void*                        pvData;
} PVR_DEBUGFS_FILE;

static struct dentry* gpsPVRDebugFSEntryDir;

static IMG_BOOL _RefDebugFSDir(PVR_DEBUGFS_DIR *psDebugFSDir);
static void     _UnrefAndMaybeDestroyDebugFSDir(PVR_DEBUGFS_DIR **ppsDebugFSDir);
static IMG_BOOL _RefDebugFSFile(PVR_DEBUGFS_FILE *psDebugFSFile);
static void     _UnrefAndMaybeDestroyDebugFSFile(PVR_DEBUGFS_FILE **ppsDebugFSFile);


static void _StatsSeqPrintf(void *pvFile, const IMG_CHAR *pszFormat, ...)
{
	IMG_CHAR szBuffer[PVR_MAX_DEBUG_MESSAGE_LEN];
	va_list  ArgList;

	va_start(ArgList, pszFormat);
	vsnprintf(szBuffer, PVR_MAX_DEBUG_MESSAGE_LEN, pszFormat, ArgList);
	seq_printf((struct seq_file *)pvFile, "%s", szBuffer);
	va_end(ArgList);
}

static int _DebugFSStatisticSeqShow(struct seq_file *psSeqFile, void *pvData)
{
	PVR_DEBUGFS_FILE *psDebugFSFile = (PVR_DEBUGFS_FILE *)psSeqFile->private;

	if (psDebugFSFile != NULL)
	{
		psDebugFSFile->pfnStatsPrint((void*)psSeqFile, psDebugFSFile->pvData, _StatsSeqPrintf);
		return 0;
	}
	else
	{
		PVR_DPF((PVR_DEBUGFS_PVR_DPF_LEVEL,
			 "%s: Called when psDebugFSFile is NULL, returning -ENODATA(%d)",
			 __func__, -ENODATA));
	}

	return -ENODATA;
}

/*************************************************************************/ /*!
 Common internal API
*/ /**************************************************************************/

#define _DRIVER_THREAD_ENTER() \
	do { \
		PVRSRV_ERROR eLocalError = PVRSRVDriverThreadEnter(); \
		if (eLocalError != PVRSRV_OK) \
		{ \
			PVR_DPF((PVR_DBG_ERROR, "%s: PVRSRVDriverThreadEnter failed: %s", \
				__func__, PVRSRVGetErrorString(eLocalError))); \
			return OSPVRSRVToNativeError(eLocalError); \
		} \
	} while (0)

#define _DRIVER_THREAD_EXIT() \
	PVRSRVDriverThreadExit()

static int _DebugFSFileOpen(struct inode *psINode, struct file *psFile)
{
	PVR_DEBUGFS_FILE *psDebugFSFile;
	int iResult = -EIO;

	_DRIVER_THREAD_ENTER();

	PVR_ASSERT(psINode);
	psDebugFSFile = (PVR_DEBUGFS_FILE *)psINode->i_private;

	if (psDebugFSFile != NULL)
	{
		/* Take ref on stat entry before opening seq file - this ref will
		 * be dropped if we fail to open the seq file or when we close it
		 */
		if (_RefDebugFSFile(psDebugFSFile))
		{
			if (psDebugFSFile->psReadOps != NULL)
			{
				iResult = seq_open(psFile, psDebugFSFile->psReadOps);

				if (iResult == 0)
				{
					struct seq_file *psSeqFile = psFile->private_data;

					psSeqFile->private = psDebugFSFile->pvData;
				}
			}
			else
			{
				iResult = single_open(psFile, _DebugFSStatisticSeqShow, psDebugFSFile);
			}

			if (iResult != 0)
			{
				/* Drop ref if we failed to open seq file */
				_UnrefAndMaybeDestroyDebugFSFile(&psDebugFSFile);

				PVR_DPF((PVR_DBG_ERROR, "%s: Failed to seq_open psFile, returning %d",
					 __func__, iResult));
			}
		}
	}
	else
	{
		mutex_unlock(&gDebugFSHashAndRefLock);

		PVR_DPF((PVR_DEBUGFS_PVR_DPF_LEVEL,
			 "%s: Called when psDebugFSFile is NULL", __func__));
	}

	_DRIVER_THREAD_EXIT();

	return iResult;
}

static int _DebugFSFileClose(struct inode *psINode, struct file *psFile)
{
	int iResult = -EIO;
	PVR_DEBUGFS_FILE *psDebugFSFile = (PVR_DEBUGFS_FILE *)psINode->i_private;

	if (psDebugFSFile != NULL)
	{
		_DRIVER_THREAD_ENTER();

		if (psDebugFSFile->psReadOps != NULL)
		{
			iResult = seq_release(psINode, psFile);
		}
		else
		{
			iResult = single_release(psINode, psFile);
		}

		if (iResult != 0)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Failed to release psFile, returning %d",
			__func__, iResult));
		}

		_UnrefAndMaybeDestroyDebugFSFile(&psDebugFSFile);

		_DRIVER_THREAD_EXIT();
	}
	else
	{
		PVR_DPF((PVR_DEBUGFS_PVR_DPF_LEVEL,
			 "%s: Called when psDebugFSFile is NULL", __func__));
	}

	return iResult;
}

static ssize_t _DebugFSFileRead(struct file *psFile,
				char __user *pszBuffer,
				size_t uiCount,
				loff_t *puiPosition)
{
	ssize_t iResult;

	_DRIVER_THREAD_ENTER();

	iResult = seq_read(psFile, pszBuffer, uiCount, puiPosition);

	if (iResult < 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to read psFile, returning %zd",
		__func__, iResult));
	}

	_DRIVER_THREAD_EXIT();

	return iResult;
}

static loff_t _DebugFSFileLSeek(struct file *psFile,
				loff_t iOffset,
				int iOrigin)
{
	loff_t iResult;

	_DRIVER_THREAD_ENTER();

	iResult = seq_lseek(psFile, iOffset, iOrigin);

	if (iResult < 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Failed to reposition offset of psFile, returning %lld",
		__func__, iResult));
	}

	_DRIVER_THREAD_EXIT();

	return iResult;
}

static ssize_t _DebugFSFileWrite(struct file *psFile,
				 const char __user *pszBuffer,
				 size_t uiCount,
				 loff_t *puiPosition)
{
	struct inode *psINode = psFile->f_path.dentry->d_inode;
	PVR_DEBUGFS_FILE *psDebugFSFile = (PVR_DEBUGFS_FILE *)psINode->i_private;
	ssize_t iResult = -EIO;

	if (psDebugFSFile != NULL)
	{
		if (psDebugFSFile->pfnWrite == NULL)
		{
			PVR_DPF((PVR_DEBUGFS_PVR_DPF_LEVEL, "%s: Called for file '%s', "
				"which does not have pfnWrite defined, returning -EIO(%d)",
				__func__, psFile->f_path.dentry->d_iname, -EIO));
			goto exit;
		}

		_DRIVER_THREAD_ENTER();

		iResult = psDebugFSFile->pfnWrite(pszBuffer, uiCount, puiPosition, psDebugFSFile->pvData);

		if (iResult < 0)
		{
			PVR_DPF((PVR_DBG_ERROR, "%s: Failed to write to psFile, returning %zd",
			__func__, iResult));
		}

		_DRIVER_THREAD_EXIT();
	}
	else
	{
		PVR_DPF((PVR_DEBUGFS_PVR_DPF_LEVEL,
			 "%s: Called when psDebugFSFile is NULL", __func__));
	}

exit:
	return iResult;
}

static const struct file_operations gsPVRDebugFSFileOps =
{
	.owner = THIS_MODULE,
	.open = _DebugFSFileOpen,
	.read = _DebugFSFileRead,
	.write = _DebugFSFileWrite,
	.llseek = _DebugFSFileLSeek,
	.release = _DebugFSFileClose,
};

/*****************************************************************************************************************************************************/

/*************************************************************************/ /*!
@Function       PVRDebugFSInit
@Description    Initialise PVR debugfs support. This should be called before
                using any PVRDebugFS functions.
@Return         int      On success, returns 0. Otherwise, returns an
                         error code.
*/ /**************************************************************************/
int PVRDebugFSInit(void)
{
	PVR_ASSERT(gpsPVRDebugFSEntryDir == NULL);

	mutex_init(&gDebugFSHashAndRefLock);

	gHashTable = HASH_Create(NUM_HASH_ENTRIES);
	if (gHashTable == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Cannot create Hash Table", __func__));
		return -ENOMEM;
	}

	gpsPVRDebugFSEntryDir = debugfs_create_dir(PVR_DEBUGFS_DIR_NAME, NULL);
	if (gpsPVRDebugFSEntryDir == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "%s: Cannot create '%s' debugfs root directory",
			 __func__, PVR_DEBUGFS_DIR_NAME));

		return -ENOMEM;
	}

	return 0;
}

/*************************************************************************/ /*!
@Function       PVRDebugFSDeInit
@Description    Deinitialise PVR debugfs support. This should be called only
                if PVRDebugFSInit() has already been called. All debugfs
                directories and entries should be removed otherwise this
                function will fail.
@Return         void
*/ /**************************************************************************/
void PVRDebugFSDeInit(void)
{
	if (gpsPVRDebugFSEntryDir != NULL)
	{
		debugfs_remove(gpsPVRDebugFSEntryDir);
		gpsPVRDebugFSEntryDir = NULL;

		HASH_Delete(gHashTable);
		gHashTable = NULL;

		mutex_destroy(&gDebugFSHashAndRefLock);
	}
}

/*****************************************************************************************************************************************************/

/*************************************************************************/ /*!
@Function       PVRDebugFSCreateEntryDir
@Description    Create a directory for debugfs entries that will be located
                under the root directory, as created by
                PVRDebugFSCreateEntries().
@Input          pszDirName       String containing the name for the directory.
@Input          psParentDir      The parent directory in which to create the new
                                 directory. This should either be NULL, meaning it
                                 should be created in the root directory, or a
                                 pointer to a directory as returned by this
                                 function.
@Output	        ppsNewDir        On success, points to the newly created
                                 directory.
@Return         int              On success, returns 0. Otherwise, returns an
                                 error code.
*/ /**************************************************************************/
int PVRDebugFSCreateEntryDir(const IMG_CHAR *pszDirName,
		    PVR_DEBUGFS_DIR *psParentDir,
		    PVR_DEBUGFS_DIR **ppsNewDir)
{
	PVR_DEBUGFS_DIR *psNewDir;
	struct dentry *psDirEntry;

	PVR_ASSERT(gpsPVRDebugFSEntryDir != NULL);
	if (pszDirName == NULL || ppsNewDir == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "%s: Invalid parameter", __func__));
		return -EINVAL;
	}

	psNewDir = OSAllocMemNoStats(sizeof(*psNewDir));
	if (psNewDir == NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "%s: Cannot allocate memory for '%s' pvr_debugfs structure",
			 __func__, pszDirName));
		return -ENOMEM;
	}

	psNewDir->psParentDir = psParentDir;
	psDirEntry = debugfs_create_dir(pszDirName, (psNewDir->psParentDir) ?
		     psNewDir->psParentDir->psDirEntry : gpsPVRDebugFSEntryDir);

	if (IS_ERR_OR_NULL(psDirEntry))
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "%s: Cannot create '%s' debugfs directory",
			 __func__, pszDirName));

		OSFreeMemNoStats(psNewDir);
		return (NULL == psDirEntry) ? -ENOMEM : -ENODEV;
	}

	psNewDir->psDirEntry = psDirEntry;
	*ppsNewDir = psNewDir;
	psNewDir->ui32DirRefCount = 1;

	/* if parent directory is not gpsPVRDebugFSEntryDir, increment its refCount */
	if (psNewDir->psParentDir != NULL)
	{
		/* if we fail to acquire the reference that probably means that
		 * parent dir was already freed - we have to cleanup in this situation */
		if (!_RefDebugFSDir(psNewDir->psParentDir))
		{
			_UnrefAndMaybeDestroyDebugFSDir(ppsNewDir);
			return -EFAULT;
		}
	}

	mutex_lock(&gDebugFSHashAndRefLock);
	if (!HASH_Insert(gHashTable, (uintptr_t)psNewDir, (uintptr_t)psNewDir))
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "%s: Failed to add Hash entry for '%s' debugfs directory",
			 __func__, pszDirName));

		mutex_unlock(&gDebugFSHashAndRefLock);
		_UnrefAndMaybeDestroyDebugFSDir(ppsNewDir);
		return -ENOMEM;
	}
	mutex_unlock(&gDebugFSHashAndRefLock);

	return 0;
}

/*************************************************************************/ /*!
@Function       PVRDebugFSRemoveEntryDir
@Description    Remove a directory that was created by
                PVRDebugFSCreateEntryDir(). Any directories or files created
                under the directory being removed should be removed first.
@Input          ppsDir       Pointer representing the directory to be removed.
                             Has to be double pointer to avoid possible races
                             and use-after-free situations.
@Return         void
*/ /**************************************************************************/
void PVRDebugFSRemoveEntryDir(PVR_DEBUGFS_DIR **ppsDir)
{
	_UnrefAndMaybeDestroyDebugFSDir(ppsDir);
}

static IMG_BOOL _RefDebugFSDir(PVR_DEBUGFS_DIR *psDebugFSDir)
{
	IMG_BOOL bStatus = IMG_FALSE;
	uintptr_t uiHashVal;

	PVR_ASSERT(psDebugFSDir != NULL && psDebugFSDir->psDirEntry != NULL);

	mutex_lock(&gDebugFSHashAndRefLock);

	uiHashVal = HASH_Retrieve(gHashTable, (uintptr_t)psDebugFSDir);
	if (uiHashVal == 0)
	{
		PVR_DPF((PVR_DEBUGFS_PVR_DPF_LEVEL,
			 "%s: Directory (%p) is already deleted, abort read",
			 __func__, psDebugFSDir));
		goto exit;
	}
	PVR_ASSERT(uiHashVal == (uintptr_t)psDebugFSDir);

	if (psDebugFSDir->ui32DirRefCount > 0)
	{
		/* Increment refCount */
		psDebugFSDir->ui32DirRefCount++;
		bStatus = IMG_TRUE;
	}
	else
	{
		PVR_DPF((PVR_DEBUGFS_PVR_DPF_LEVEL, "%s: Called ref on psDebugFSDir '%s'"
			" when ui32RefCount is zero", __func__,
			psDebugFSDir->psDirEntry->d_iname));
	}

exit:
	mutex_unlock(&gDebugFSHashAndRefLock);

	return bStatus;
}

/* decrements refCount on a directory and removes it if the count reaches
 * 0, this function also walks recursively over parent directories and
 * decrements refCount on them too
 * note: it's safe to call this function with *ppsDebugFSDir pointing to NULL */
static void _UnrefAndMaybeDestroyDebugFSDir(PVR_DEBUGFS_DIR **ppsDebugFSDir)
{
	PVR_DEBUGFS_DIR *psDebugFSDir, *psParentDir = NULL;
	struct dentry *psDir = NULL;

	PVR_ASSERT(ppsDebugFSDir != NULL);

	psDebugFSDir = *ppsDebugFSDir;

	/* it's ok to call this function with NULL pointer */
	if (psDebugFSDir == NULL)
	{
		return;
	}

	mutex_lock(&gDebugFSHashAndRefLock);

	PVR_ASSERT(psDebugFSDir->psDirEntry != NULL);

	if (psDebugFSDir->ui32DirRefCount > 0)
	{
		/* Decrement refCount and free if now zero */
		if (--psDebugFSDir->ui32DirRefCount == 0)
		{
			uintptr_t uiHashVal;

			uiHashVal = HASH_Remove(gHashTable, (uintptr_t)psDebugFSDir);
			if (uiHashVal == 0)
			{
				PVR_DPF((PVR_DEBUGFS_PVR_DPF_LEVEL,
					 "%s: Entry for Dir '%s'' not found in Hash table",
					 __func__, psDebugFSDir->psDirEntry->d_iname));
			}
			else
			{
				PVR_ASSERT(uiHashVal == (uintptr_t)psDebugFSDir);
			}

			psDir = psDebugFSDir->psDirEntry;
			psParentDir = psDebugFSDir->psParentDir;

			psDebugFSDir->psDirEntry = NULL;
			psDebugFSDir->psParentDir = NULL;

			*ppsDebugFSDir = NULL;

			OSFreeMemNoStats(psDebugFSDir);
		}
	}
	else
	{
		PVR_DPF((PVR_DEBUGFS_PVR_DPF_LEVEL, "%s: Called to unref psDebugFSDir '%s'"
			" when ui32RefCount is zero", __func__,
			psDebugFSDir->psDirEntry->d_iname));
	}

	/* unlock here so we don't have any relation with the locks that might
	 * be taken in debugfs_remove() */
	mutex_unlock(&gDebugFSHashAndRefLock);

	if (psDir != NULL)
	{
		debugfs_remove(psDir);
	}

	/* decrement refcount of parent directory */
	if (psParentDir != NULL)
	{
		_UnrefAndMaybeDestroyDebugFSDir(&psParentDir);
	}
}

/*****************************************************************************************************************************************************/

/*************************************************************************/ /*!
@Function               PVRDebugFSCreateFile
@Description            Create an entry in the specified directory.
@Input                  pszName        String containing the name for the entry.
@Input                  psParentDir    Pointer from PVRDebugFSCreateEntryDir()
                                       representing the directory in which to create
                                       the entry or NULL for the root directory.
@Input                  psReadOps      Pointer to structure containing the necessary
                                       functions to read from the entry.
@Input                  pfnWrite       Callback function used to write to the entry.
                                       This function must update the offset pointer
                                       before it returns.
@Input                  pfnStatsPrint  A callback function used to print all the
                                       statistics when reading from the statistic
                                       entry.
@Input                  pvData         Private data to be passed to the read
                                       functions, in the seq_file private member, and
                                       the write function callback.
@Output                 ppsNewFile     On success, points to the newly created entry.
@Return                 int            On success, returns 0. Otherwise, returns an
                                       error code.
*/ /**************************************************************************/
int PVRDebugFSCreateFile(const char *pszName,
			  PVR_DEBUGFS_DIR *psParentDir,
			  const struct seq_operations *psReadOps,
			  PVRSRV_ENTRY_WRITE_FUNC *pfnWrite,
			  OS_STATS_PRINT_FUNC *pfnStatsPrint,
			  void *pvData,
			  PVR_DEBUGFS_FILE **ppsNewFile)
{
	PVR_DEBUGFS_FILE *psDebugFSFile;
	struct dentry *psEntry;
	umode_t uiMode;

	PVR_ASSERT(gpsPVRDebugFSEntryDir != NULL);

	psDebugFSFile = OSAllocMemNoStats(sizeof(*psDebugFSFile));
	if (psDebugFSFile == NULL)
	{
		return -ENOMEM;
	}

	psDebugFSFile->psReadOps = psReadOps;
	psDebugFSFile->pfnWrite = pfnWrite;
	psDebugFSFile->pvData = pvData;
	psDebugFSFile->pfnStatsPrint = pfnStatsPrint;

	uiMode = S_IFREG;

	if (psReadOps != NULL)
	{
		uiMode |= S_IRUGO;
	}

	if (pfnWrite != NULL)
	{
		uiMode |= S_IWUSR;
	}

	psDebugFSFile->psParentDir = psParentDir;
	psDebugFSFile->ui32FileRefCount = 1;

	psEntry = debugfs_create_file(pszName,
				      uiMode,
				      (psParentDir != NULL) ? psParentDir->psDirEntry : gpsPVRDebugFSEntryDir,
				      psDebugFSFile,
				      &gsPVRDebugFSFileOps);
	if (IS_ERR_OR_NULL(psEntry))
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "%s: Cannot create debugfs '%s' file",
			 __func__, pszName));

		OSFreeMemNoStats(psDebugFSFile);
		return (NULL == psEntry) ? -ENOMEM : -ENODEV;
	}

	psDebugFSFile->psFileEntry = psEntry;
	if (ppsNewFile != NULL)
	{
		*ppsNewFile = (void*)psDebugFSFile;
	}

	if (psDebugFSFile->psParentDir != NULL)
	{
		/* increment refCount of parent directory */
		if (!_RefDebugFSDir(psDebugFSFile->psParentDir))
		{
			_UnrefAndMaybeDestroyDebugFSFile(ppsNewFile);
			return -EFAULT;
		}
	}

	mutex_lock(&gDebugFSHashAndRefLock);
	if (!HASH_Insert(gHashTable, (uintptr_t)psDebugFSFile, (uintptr_t)psDebugFSFile))
	{
		PVR_DPF((PVR_DBG_ERROR,
			 "%s: Failed to add Hash entry for '%s' debugfs file",
			 __func__, pszName));

		mutex_unlock(&gDebugFSHashAndRefLock);
		_UnrefAndMaybeDestroyDebugFSFile(ppsNewFile);
		return -ENOMEM;
	}
	mutex_unlock(&gDebugFSHashAndRefLock);

	return 0;
}

/*************************************************************************/ /*!
@Function       PVRDebugFSRemoveFile
@Description    Removes an entry that was created by PVRDebugFSCreateFile().
@Input          ppsDebugFSFile   Pointer representing the entry to be removed.
                Has to be double pointer to avoid possible races
                and use-after-free situations.
@Return         void
*/ /**************************************************************************/
void PVRDebugFSRemoveFile(PVR_DEBUGFS_FILE **ppsDebugFSFile)
{
	_UnrefAndMaybeDestroyDebugFSFile(ppsDebugFSFile);
}


static IMG_BOOL _RefDebugFSFile(PVR_DEBUGFS_FILE *psDebugFSFile)
{
	IMG_BOOL bResult = IMG_FALSE;
	uintptr_t uiHashVal;

	mutex_lock(&gDebugFSHashAndRefLock);

	PVR_ASSERT(psDebugFSFile != NULL);

	uiHashVal = HASH_Retrieve(gHashTable, (uintptr_t)psDebugFSFile);
	if (uiHashVal == 0)
	{
		PVR_DPF((PVR_DEBUGFS_PVR_DPF_LEVEL, "%s: File (%p) is already deleted",
			 __func__, psDebugFSFile));

		return IMG_FALSE;
	}
	PVR_ASSERT(uiHashVal == (uintptr_t)psDebugFSFile);

	if (psDebugFSFile->ui32FileRefCount > 0)
	{
		/* Increment refCount of psDebugFSFile */
		psDebugFSFile->ui32FileRefCount++;
		bResult = IMG_TRUE;
	}
	else
	{
		PVR_DPF((PVR_DEBUGFS_PVR_DPF_LEVEL, "%s: Called ref on psDebugFSFile '%s'"
			" when ui32FileRefCount is zero", __func__,
			psDebugFSFile->psFileEntry->d_iname));
	}

	mutex_unlock(&gDebugFSHashAndRefLock);

	return bResult;
}

static void _UnrefAndMaybeDestroyDebugFSFile(PVR_DEBUGFS_FILE **ppsDebugFSFile)
{
	PVR_DEBUGFS_FILE *psDebugFSFile;
	PVR_DEBUGFS_DIR *psParentDir = NULL;
	struct dentry *psEntry = NULL;

	mutex_lock(&gDebugFSHashAndRefLock);

	/* Decrement refCount of psDebugFSFile, and free if now zero */
	psDebugFSFile = *ppsDebugFSFile;
	PVR_ASSERT(psDebugFSFile != NULL);

	if (psDebugFSFile->ui32FileRefCount > 0)
	{
		if (--psDebugFSFile->ui32FileRefCount == 0)
		{
			uintptr_t uiHashVal;

			uiHashVal = HASH_Remove(gHashTable, (uintptr_t)psDebugFSFile);
			if (uiHashVal == 0)
			{
				PVR_DPF((PVR_DEBUGFS_PVR_DPF_LEVEL,
					 "%s: Entry for File '%s'' not found in Hash table",
					 __func__, psDebugFSFile->psFileEntry->d_iname));
			}
			else
			{
				PVR_ASSERT(uiHashVal == (uintptr_t)psDebugFSFile);
			}

			psEntry = psDebugFSFile->psFileEntry;
			psParentDir = psDebugFSFile->psParentDir;

			if (psEntry != NULL)
			{
				/* set to NULL so nothing can reference this pointer, we have
				 * a copy that will be used to free the memory */
				*ppsDebugFSFile = NULL;

				psEntry->d_inode->i_private = NULL;
			}

			/* now free the memory allocated for psDebugFSFile */
			OSFreeMemNoStats(psDebugFSFile);
		}
	}
	else
	{
		PVR_DPF((PVR_DEBUGFS_PVR_DPF_LEVEL, "%s: Called to unref psDebugFSFile"
			" '%s' when ui32RefCount is zero", __func__,
			psDebugFSFile->psFileEntry->d_iname));
	}

	/* unlock here so we don't have any relation with the locks that might
	 * be taken in debugfs_remove() */
	mutex_unlock(&gDebugFSHashAndRefLock);

	if (psEntry != NULL)
	{
		/* we should be able to do it outside of the lock now since
		 * even if something opens the file the private data is already
		 * NULL*/
		debugfs_remove(psEntry);
	}

	if (psParentDir != NULL)
	{
		/* decrement refcount of parent directory */
		_UnrefAndMaybeDestroyDebugFSDir(&psParentDir);
	}
}

