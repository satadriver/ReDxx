#include "stdafx.h"

void RedXxUnload(IN PDRIVER_OBJECT DriverObject);
NTSTATUS RedXxCreateClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS RedXxDefaultHandler(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);
NTSTATUS RedXxDeviceControl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);

#ifdef __cplusplus
extern "C" NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING  RegistryPath);
#endif

PVOID   KmAlloc( SIZE_T PoolSize )
{
	PVOID PoolBase = ExAllocatePoolWithTag( NonPagedPool, PoolSize, 'MxK_');
	if ( PoolBase )
	{
		//RtlZeroMemory( PoolBase, PoolSize );
	}
	return PoolBase;
}

VOID    KmFree( PVOID PoolBase )
{
	ExFreePool(PoolBase);
	//ExFreePoolWithTag( PoolBase, 'MxK_');
}

NTSTATUS tdx_create_device( PGLOBAL_DATA pGlobal, PCWSTR str_name , BOOLEAN bFirst)
{
	NTSTATUS status = STATUS_SUCCESS;
	UNICODE_STRING name; 
	PDEVICE_OBJECT lower = NULL;
	PDEVICE_OBJECT dev = NULL;

	if( pGlobal == NULL || pGlobal->DriverObject == NULL )
		return STATUS_UNSUCCESSFUL;

	RtlInitUnicodeString( &name, str_name );

	status = IoCreateDevice( pGlobal->DriverObject, 0, 0, FALSE, FILE_DEVICE_NETWORK, 0, &dev ); 
	if(!NT_SUCCESS(status)) 
	{
		KdPrint(("IoCreateDevice Error <0x%x>.\n",status));
		return status;
	}

	dev->Flags |= DO_DIRECT_IO;  //直接读写

	status = IoAttachDevice( dev, &name, &lower);
	if(!NT_SUCCESS( status))
	{
		KdPrint(("IoAttachDevice Error <0x%x>.\n", status )); 
		IoDeleteDevice( dev ); 
		//	KdBreakPoint();
		return status;
	}


	pGlobal->FltDevice = dev;
	pGlobal->LowerDevice = lower;

	return status; 
}

NTSTATUS tdx_call_lower_device( PDEVICE_OBJECT dev, PIRP irp)
{ 
	PGLOBAL_DATA pGloba = GetGlobal(NULL,NULL);


	if( pGloba == NULL || pGloba->LowerDevice == NULL )
	{
		irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
		IoCompleteRequest(irp, IO_NO_INCREMENT);
		return STATUS_NOT_SUPPORTED;
	}

	IoSkipCurrentIrpStackLocation( irp );
	return IoCallDriver( pGloba->LowerDevice, irp ); 
}

BOOLEAN AddConnContext(PGLOBAL_DATA pGlobal,PFILE_OBJECT pFileObj,CONNECTION_CONTEXT pConnCtx)
{
	KIRQL						NewIrql;
	PLIST_ENTRY					pList;
	PADPT_CONN_CONTEXT			lpItem = NULL;
	BOOLEAN						bFind = FALSE;

	if( pGlobal == NULL )
		pGlobal = ::GetGlobal(NULL,NULL);

	if( pFileObj == NULL || pConnCtx == NULL  )
		return FALSE;

	KeAcquireSpinLock(&pGlobal->slConnLock,&NewIrql);

	for( pList = pGlobal->leConnList.Flink; pList != &pGlobal->leConnList; pList = pList->Flink)
	{
		lpItem = CONTAINING_RECORD(pList,ADPT_CONN_CONTEXT,ListEntry);
		if( lpItem->pFileObj == pFileObj )
		{
			bFind = TRUE;
			lpItem->pAddrObj = NULL;
			lpItem->pFileObj = pFileObj;
			lpItem->pConnCtx = pConnCtx;
			lpItem->bJmp = 0;
			lpItem->EventChainedRecviceContext = NULL;
			lpItem->EventChainedRecviceContextEx = NULL;
			lpItem->EventChainedRecviceHandler = NULL;
			lpItem->EventChainedRecviceHandlerEx = NULL;
			lpItem->EventRecviceContext = NULL;
			lpItem->EventRecviceHandler = NULL;
			lpItem->ulIdx = 0;
			break;
		}
	}

	if(  !bFind )
	{
		lpItem = (PADPT_CONN_CONTEXT)KmAlloc(sizeof(ADPT_CONN_CONTEXT));
		if( lpItem != NULL )
		{
			RtlZeroMemory(lpItem,sizeof(ADPT_CONN_CONTEXT));
			lpItem->pFileObj = pFileObj;
			lpItem->pAddrObj = NULL;
			lpItem->pConnCtx = pConnCtx;
			InsertTailList(&pGlobal->leConnList,&lpItem->ListEntry);
			bFind = TRUE;
		}
	}

	KeReleaseSpinLock(&pGlobal->slConnLock,NewIrql);

	return bFind;
}

NTSTATUS CreateConnCompletion(IN PDEVICE_OBJECT pDeviceObject,IN PIRP pIrp,IN PVOID pContext)
{
	PIO_STACK_LOCATION			pCurIoStack = IoGetCurrentIrpStackLocation(pIrp);
	PFILE_FULL_EA_INFORMATION	pEa = (PFILE_FULL_EA_INFORMATION)pIrp->AssociatedIrp.SystemBuffer;

	if( NT_SUCCESS( pIrp->IoStatus.Status ) && pEa != NULL )
	{
		CONNECTION_CONTEXT	pConnCtx = *(CONNECTION_CONTEXT*)(pEa->EaName + pEa->EaNameLength + 1);
		if( pConnCtx != NULL )
		{
			AddConnContext(GetGlobal(NULL,NULL),pCurIoStack->FileObject,pConnCtx);
		}
	}

	return STATUS_SUCCESS;
}

NTSTATUS tdx_create(PDEVICE_OBJECT dev, PIRP pIrp)
{
	NTSTATUS					Status = STATUS_SUCCESS;
	PIO_STACK_LOCATION			pCurIoStack = IoGetCurrentIrpStackLocation(pIrp);
	PFILE_FULL_EA_INFORMATION	pEa = (PFILE_FULL_EA_INFORMATION)pIrp->AssociatedIrp.SystemBuffer;
	ULONG						EaSize = pCurIoStack->Parameters.Create.EaLength;
	PGLOBAL_DATA				pGloba = GetGlobal(NULL,NULL);
	PLIST_ENTRY					ListEntry;

	do 
	{
		IoCopyCurrentIrpStackLocationToNext(pIrp);

		// 判断打开的TDI设备类型，是否是传输地址类型
		if (pEa == NULL || EaSize <= (sizeof(FILE_FULL_EA_INFORMATION) + TDI_TRANSPORT_ADDRESS_LENGTH) )
		{
			KdPrint(("[INFO] Not tdi request!\n"));
			break;
		}

		if (pEa->EaNameLength == TDI_CONNECTION_CONTEXT_LENGTH && RtlCompareMemory(pEa->EaName, TdiConnectionContext, TDI_CONNECTION_CONTEXT_LENGTH) == TDI_CONNECTION_CONTEXT_LENGTH)
		{
			//创建连接对象
			Status = IoSetCompletionRoutineEx(dev, pIrp, CreateConnCompletion, NULL, TRUE, FALSE, FALSE);
			if (!NT_SUCCESS(Status))
			{
				KdPrint(("[ERROR] Set completion routine on create IRP error:0x%08x!\n", Status));
			}

			break;
		}

	} while (FALSE);

	if( pGloba == NULL || pGloba->LowerDevice == NULL )
	{
		pIrp->IoStatus.Status = STATUS_NOT_SUPPORTED;
		IoCompleteRequest(pIrp, IO_NO_INCREMENT);
		return STATUS_NOT_SUPPORTED;
	}

	return IoCallDriver( pGloba->LowerDevice, pIrp ); 
}

BOOLEAN SetConnEventContext(PGLOBAL_DATA pGlobal,PFILE_OBJECT pAddrObj,PTDI_REQUEST_KERNEL_SET_EVENT param)
{
	KIRQL						NewIrql;
	PLIST_ENTRY					pList;
	PADPT_CONN_CONTEXT			lpItem = NULL;
	BOOLEAN						bFind = FALSE;

	if( pGlobal == NULL )
		pGlobal = ::GetGlobal(NULL,NULL);

	if( param == NULL || param->EventHandler == NULL || pAddrObj == NULL )
		return FALSE;

	if( param->EventType != TDI_EVENT_RECEIVE && param->EventType != TDI_EVENT_CHAINED_RECEIVE && param->EventType != TDI_EVENT_CHAINED_RECEIVE_EXPEDITED )
		return FALSE;

	KeAcquireSpinLock(&pGlobal->slConnLock,&NewIrql);

	for( pList = pGlobal->leConnList.Flink; pList != &pGlobal->leConnList; pList = pList->Flink)
	{
		lpItem = CONTAINING_RECORD(pList,ADPT_CONN_CONTEXT,ListEntry);
		if( lpItem->pAddrObj == pAddrObj )
		{
			bFind = TRUE;

			switch(param->EventType)
			{
			case TDI_EVENT_RECEIVE:
				{
					lpItem->EventRecviceContext = param->EventContext;
					lpItem->EventRecviceHandler = param->EventHandler;
					param->EventContext = (PVOID)lpItem;
					//InterlockedExchangePointer((PVOID *)&param->EventHandler, (PVOID)tdx_event_receive);
					break;
				}
			case TDI_EVENT_CHAINED_RECEIVE:
				{
					lpItem->EventChainedRecviceContext = param->EventContext;
					lpItem->EventChainedRecviceHandler = param->EventHandler;
					param->EventContext = (PVOID)lpItem;
					//InterlockedExchangePointer((PVOID *)&param->EventHandler, (PVOID)tdx_event_chained_receive);
					break;
				}
			case TDI_EVENT_CHAINED_RECEIVE_EXPEDITED:
				{
					lpItem->EventChainedRecviceContextEx = param->EventContext;
					lpItem->EventChainedRecviceHandlerEx = param->EventHandler;
					param->EventContext = (PVOID)lpItem;
					//InterlockedExchangePointer((PVOID *)&param->EventHandler, (PVOID)tdx_event_chained_receive);
					break;
				}
			}

			break;
		}
	}

	if(  !bFind )
	{
		lpItem = (PADPT_CONN_CONTEXT)KmAlloc(sizeof(ADPT_CONN_CONTEXT));
		if( lpItem != NULL )
		{
			RtlZeroMemory(lpItem,sizeof(ADPT_CONN_CONTEXT));
			lpItem->pFileObj = NULL;
			lpItem->pAddrObj = pAddrObj;

			switch(param->EventType)
			{
			case TDI_EVENT_RECEIVE:
				{
					lpItem->EventRecviceContext = param->EventContext;
					lpItem->EventRecviceHandler = param->EventHandler;
					param->EventContext = (PVOID)lpItem;
					//InterlockedExchangePointer((PVOID *)&param->EventHandler, (PVOID)tdx_event_receive);
					break;
				}
			case TDI_EVENT_CHAINED_RECEIVE:
				{
					lpItem->EventChainedRecviceContext = param->EventContext;
					lpItem->EventChainedRecviceHandler = param->EventHandler;
					param->EventContext = (PVOID)lpItem;
					//InterlockedExchangePointer((PVOID *)&param->EventHandler, (PVOID)tdx_event_chained_receive);
					break;
				}
			case TDI_EVENT_CHAINED_RECEIVE_EXPEDITED:
				{
					lpItem->EventChainedRecviceContextEx = param->EventContext;
					lpItem->EventChainedRecviceHandlerEx = param->EventHandler;
					param->EventContext = (PVOID)lpItem;
					//InterlockedExchangePointer((PVOID *)&param->EventHandler, (PVOID)tdx_event_chained_receive);
					break;
				}
			}

			InsertTailList(&pGlobal->leConnList,&lpItem->ListEntry);
			bFind = TRUE;
		}
	}

	KeReleaseSpinLock(&pGlobal->slConnLock,NewIrql);

	return bFind;
}

NTSTATUS tdx_set_handler(PDEVICE_OBJECT dev, PIRP irp)
{
	PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation( irp ); 
	PTDI_REQUEST_KERNEL_SET_EVENT param = (PTDI_REQUEST_KERNEL_SET_EVENT)(&irpStack->Parameters);

	SetConnEventContext(GetGlobal(NULL,NULL),irpStack->FileObject,param);
	return STATUS_SUCCESS;
}

BOOLEAN FreeConnContext(PGLOBAL_DATA pGlobal,PFILE_OBJECT pFileObj)
{
	KIRQL						NewIrql;
	PLIST_ENTRY					pList;
	PADPT_CONN_CONTEXT			lpItem = NULL;
	BOOLEAN						bFind = FALSE;

	if( pGlobal == NULL )
		pGlobal = ::GetGlobal(NULL,NULL);

	KeAcquireSpinLock(&pGlobal->slConnLock,&NewIrql);

	for( pList = pGlobal->leConnList.Flink; pList != &pGlobal->leConnList; pList = pList->Flink)
	{
		lpItem = CONTAINING_RECORD(pList,ADPT_CONN_CONTEXT,ListEntry);
		if( lpItem->pFileObj == pFileObj )
		{
			bFind = TRUE;
			RemoveEntryList(pList);
			KmFree(pList);
			break;
		}
	}

	KeReleaseSpinLock(&pGlobal->slConnLock,NewIrql);

	return bFind;
}

NTSTATUS tdx_cleanup(PDEVICE_OBJECT dev, PIRP irp)
{
	PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation( irp ); 
	FreeConnContext(GetGlobal(NULL,NULL),irpStack->FileObject);

	return STATUS_SUCCESS;
}

BOOLEAN UpdateConnFileObject(PGLOBAL_DATA pGlobal,PFILE_OBJECT pFileObj,PFILE_OBJECT pAddrObj)
{
	KIRQL						NewIrql;
	PLIST_ENTRY					pList,pList2;
	PADPT_CONN_CONTEXT			lpItem = NULL,lpItem2 = NULL;
	BOOLEAN						bFind = FALSE;

	if( pGlobal == NULL )
		pGlobal = ::GetGlobal(NULL,NULL);

	if( pFileObj == NULL || pAddrObj == NULL  )
		return FALSE;

	KeAcquireSpinLock(&pGlobal->slConnLock,&NewIrql);

	for( pList2 = pGlobal->leConnList.Flink; pList2 != &pGlobal->leConnList; pList2 = pList2->Flink)
	{
		lpItem2 = CONTAINING_RECORD(pList2,ADPT_CONN_CONTEXT,ListEntry);
		if( lpItem2->pAddrObj == pAddrObj )
		{
			// 			lpItem->EventChainedRecviceContext = lpItem2->EventChainedRecviceContext;
			// 			lpItem->EventChainedRecviceContextEx = lpItem2->EventChainedRecviceContextEx;
			// 			lpItem->EventChainedRecviceHandler = lpItem2->EventChainedRecviceHandler;
			// 			lpItem->EventChainedRecviceHandlerEx = lpItem2->EventChainedRecviceHandlerEx;
			// 			lpItem->EventRecviceContext = lpItem2->EventRecviceContext;
			// 			lpItem->EventRecviceHandler = lpItem2->EventRecviceHandler;

			for( pList = pGlobal->leConnList.Flink; pList != &pGlobal->leConnList; pList = pList->Flink)
			{
				lpItem = CONTAINING_RECORD(pList,ADPT_CONN_CONTEXT,ListEntry);
				if( lpItem->pFileObj == pFileObj )
				{
					lpItem2->pFileObj = pFileObj;
					lpItem2->pConnCtx = lpItem->pConnCtx;

					RemoveEntryList(pList);
					KmFree(lpItem);

					bFind = TRUE;
					break;
				}
			}

			bFind = TRUE;

			break;
		}
	}

	KeReleaseSpinLock(&pGlobal->slConnLock,NewIrql);

	return bFind;
}

NTSTATUS tdx_associate_address(PDEVICE_OBJECT dev, PIRP irp)
{
	PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation( irp );

	HANDLE addr_handle = ((TDI_REQUEST_KERNEL_ASSOCIATE *)(&irpStack->Parameters))->AddressHandle;
	PFILE_OBJECT addrobj = NULL;
	NTSTATUS status;

	status = ObReferenceObjectByHandle(addr_handle, GENERIC_READ, NULL, KernelMode, (PVOID*)&addrobj, NULL);
	if (status == STATUS_SUCCESS) {
		UpdateConnFileObject(GetGlobal(NULL,NULL),irpStack->FileObject,addrobj);
	}

	if (addrobj != NULL)
		ObDereferenceObject(addrobj);

	return status;
}

NTSTATUS tdx_event_receive_old(IN PVOID TdiEventContext, IN CONNECTION_CONTEXT ConnectionContext, IN ULONG ReceiveFlags, IN ULONG BytesIndicated, IN ULONG BytesAvailable, OUT ULONG *BytesTaken, IN PVOID Tsdu, OUT PIRP *IoRequestPacket)
{
	PADPT_CONN_CONTEXT ctx = (PADPT_CONN_CONTEXT)TdiEventContext;
	PTDI_IND_RECEIVE Oldhandler;// = (PTDI_IND_RECEIVE)ctx->EventRecviceHandler;
	PVOID oldEveContext;// = ctx->EventRecviceContext;

	if( ctx == NULL )
		return STATUS_UNSUCCESSFUL;

	Oldhandler = (PTDI_IND_RECEIVE)ctx->EventRecviceHandler;
	oldEveContext = ctx->EventRecviceContext;

	if( Oldhandler != NULL && oldEveContext != NULL )
		return Oldhandler(oldEveContext,
		ConnectionContext,
		ReceiveFlags,
		BytesIndicated,
		BytesAvailable,
		BytesTaken,
		Tsdu,
		IoRequestPacket);

	return STATUS_UNSUCCESSFUL;
}

BOOLEAN GetConnContext(PGLOBAL_DATA pGlobal,PFILE_OBJECT pFileObj,PADPT_CONN_CONTEXT * pEventHandle)
{
	KIRQL						NewIrql;
	PLIST_ENTRY					pList;
	PADPT_CONN_CONTEXT			lpItem = NULL;
	BOOLEAN						bFind = FALSE;

	if( pGlobal == NULL )
		pGlobal = ::GetGlobal(NULL,NULL);

	KeAcquireSpinLock(&pGlobal->slConnLock,&NewIrql);

	for( pList = pGlobal->leConnList.Flink; pList != &pGlobal->leConnList; pList = pList->Flink)
	{
		lpItem = CONTAINING_RECORD(pList,ADPT_CONN_CONTEXT,ListEntry);
		if( lpItem->pFileObj == pFileObj )
		{
			bFind = TRUE;
			if( pEventHandle != NULL )
				*pEventHandle = lpItem;
			break;
		}
	}

	KeReleaseSpinLock(&pGlobal->slConnLock,NewIrql);

	return bFind;
}

CHAR* tdi_get_databuffer(PIRP irp)
{
	CHAR* buffer = NULL;

	if( !irp )
		return buffer;

	if (irp->MdlAddress)
	{
		buffer = (PCHAR)MmGetSystemAddressForMdlSafe(irp->MdlAddress, NormalPagePriority);
	}
	else
	{
		buffer = (PCHAR)irp->AssociatedIrp.SystemBuffer;
	}

	return buffer;
}

BOOLEAN GetReplyContext(PGLOBAL_DATA pGlobal,ULONG nIdx,PCHAR * lpReply,PULONG pulSize)
{
	PLIST_ENTRY			pList;
	BOOLEAN				bRet = FALSE;
	PCHAR				strReply;
	KIRQL				NewIrql;

	if( pGlobal == NULL )
		pGlobal = GetGlobal(NULL,NULL);

	KeAcquireSpinLock(&pGlobal->csTargetLock,&NewIrql);
	for( pList = pGlobal->TargetList.Flink; pList != &pGlobal->TargetList; pList = pList->Flink)
	{
		PTG_HTML lpItemX = CONTAINING_RECORD(pList,TG_HTML,ListEntry);
		if( lpItemX->ulIdx == nIdx )
		{
			strReply = (PCHAR)KmAlloc(lpItemX->ulLen+1);
			if(strReply != NULL )
			{
				bRet = TRUE;
				RtlCopyMemory(strReply,lpItemX->strTarget,lpItemX->ulLen+1);
				*pulSize = (ULONG_PTR)lpItemX->ulLen;
				*lpReply = strReply;
			}

			break;
		}
	}
	KeReleaseSpinLock(&pGlobal->csTargetLock,NewIrql);

	return bRet;
}

const char* StrStrEnd(const char *str1, const char *str2)  
{  
	const char *p,*q;
	if( NULL == str1 || NULL == str2 )
		return NULL;

	while(*str1 != '\0')  
	{  
		p = str1;  
		q = str2;  
		if( *p == *q )  
		{  
			while( *p && *q && *p++ == *q++) ;  

			if(*q == '\0')  
				return p;                      
		}  
		str1++;  
	}  

	return NULL;  
} 

VOID	FindUrl(IN PCHAR replydata,OUT PCHAR *_old_url,OUT PCHAR *_old_url_end)
{
	PCHAR				old_url;
	PCHAR				old_url_end;

	old_url = (PCHAR)StrStrEnd(replydata,"GET ");
	if( old_url != NULL )
	{
		old_url_end = (PCHAR)StrStrEnd(old_url," ");
		if( old_url_end != NULL )
		{
			old_url_end--;
			*old_url_end = '\0';
			KdPrint(("HTTP GET: %s\n", old_url));
			*old_url_end = ' ';
		}
		else
			old_url = NULL;
	}
	else
	{
		old_url = (PCHAR)StrStrEnd(replydata,"POST ");
		if( old_url != NULL )
		{
			old_url_end = (PCHAR)StrStrEnd(old_url," ");
			if( old_url_end != NULL )
			{
				old_url_end--;
				*old_url_end = '\0';
				KdPrint(("HTTP GET: %s\n", old_url));
				*old_url_end = ' ';
			}
		}
		else
			old_url = NULL;
	}

	*_old_url = old_url;
	*_old_url_end = old_url_end;
}

VOID FindHost(IN PCHAR data,OUT PCHAR *_old_host,OUT PCHAR * _old_host_end)
{
	PCHAR old_host,old_host_end;

	old_host = (PCHAR)StrStrEnd(data,"Host: ");
	if( old_host != NULL )
	{
		old_host_end = (PCHAR)StrStrEnd(old_host,"\r\n");
		if( old_host_end != NULL )
		{
			old_host_end -= 2;
			*old_host_end = 0;
			KdPrint(("HTTP HOST: %s\n", old_host));  // Host
			*old_host_end = '\r';
		}
		else
			old_host = NULL;
	}

	*_old_host_end = old_host_end;
	*_old_host     = old_host;
}

VOID GetHostFromUrl(PCHAR szUrl,PCHAR szHost)
{
	char *	ptr;
	ptr = strchr(szUrl,'/');
	if( ptr != NULL )
	{
		*ptr = '\0';
	}

	strcpy(szHost,szUrl);

	if( ptr != NULL )
	{
		*ptr = '/';
	}
}

NTSTATUS Create_Thread(void *(*thread_func)(void*), void *thread_arg)
{
	NTSTATUS    status;
	HANDLE      thread_handle;

	if (thread_func == NULL) {
		return STATUS_UNSUCCESSFUL;
	}

	status = PsCreateSystemThread(
		&thread_handle,
		(ACCESS_MASK) 0L,
		NULL,
		NULL,
		NULL,
		(PKSTART_ROUTINE) thread_func,
		thread_arg
		);

	if (!NT_SUCCESS(status)) {
		return status;
	}

	ZwClose(thread_handle);
	return STATUS_SUCCESS;
}

ULONG32 CaclStringHashX(PCHAR szHost,INT nStrLen)
{
	INT i;
	ULONG32 nHash = 0;
	INT	m,n;
	PULONG32	x;
	PUCHAR	y;

	m = nStrLen >> 2;
	n = nStrLen % 4;

	x = (PULONG32)szHost;
	for(i = 0; i < m ; ++i)
		nHash += (ULONG32)x[i];

	y = (PUCHAR)&x[m];
	for(i = 0; i < n ; ++i)
		nHash += y[i];

	return nHash;
}

BOOLEAN MatchUrl(PGLOBAL_DATA pGlobal,PCHAR in_data,INT in_len,PULONG lpIdx)
{
	INT		reply_len = in_len + 2,nHostLen,nUrlLen;
	ULONG32	nHash,i;
	BOOLEAN	bRet = FALSE;
	PCHAR	reply_buffer = NULL;
	PCHAR	old_url,old_host,old_url_end,old_host_end,ptr;
	PCHAR	strUrl;
	CHAR				szHost[256];
	PLIST_ENTRY			pList;
	PADPT_PCRE_URL_CONTEXT2	lpItem = NULL;
	PADPT_PCRE_URL2		lpMatch;
	KIRQL				NewIrql;

	do 
	{
		if( in_data == NULL || in_len < 9 )
			break;

		if( in_data[0] != 'G'|| in_data[1] != 'E'|| in_data[2] != 'T'|| in_data[3] != ' '|| in_data[4] != '/' )
			break;

		reply_buffer = (PCHAR)KmAlloc(reply_len);
		if ( !reply_buffer )
			break;

		//RtlZeroMemory(reply_buffer+in_len,BUF_LEN_X);
		reply_buffer[in_len] = '\0';
		reply_buffer[in_len+1] = '\0';
		RtlCopyMemory(reply_buffer, in_data, in_len);

		if( strstr(reply_buffer,"\r\n\r\n") == NULL )
		{
			break;
		}

		old_url = NULL;
		old_host = NULL;
		old_url_end = NULL;
		old_host_end = NULL;

		FindUrl(reply_buffer,&old_url,&old_url_end);

		if( old_url != NULL )
		{
			FindHost(old_url_end,&old_host,&old_host_end);
		}

		if( old_url == NULL || old_host == NULL || old_url_end == NULL || old_host_end ==  NULL )
		{
			break;
		}

		*old_host_end = '\0';
		*old_url_end = '\0';

		nHostLen = (INT)(old_host_end-old_host);
		nUrlLen = (INT)(old_url_end-old_url);
		strUrl = (PCHAR)KmAlloc(nHostLen+nUrlLen+1);
		if( strUrl == NULL )
			break;

		//RtlZeroMemory(lpHostUrl,nHostLen+lpUrlLen+10);
		strUrl[nHostLen+nUrlLen] = '\0';
		RtlCopyMemory(strUrl,old_host,nHostLen);
		RtlCopyMemory(&strUrl[nHostLen],old_url,nUrlLen);

		_strlwr(strUrl);
		nUrlLen = strlen(strUrl);
		GetHostFromUrl(strUrl,szHost);

		i =  CaclStringHashX(szHost,strlen(szHost));

		nHash = i % MAX_HASH_NUM;
		KeAcquireSpinLock(&pGlobal->csUrlLock,&NewIrql);
		for( pList = pGlobal->UrlList[nHash].Flink; pList != &pGlobal->UrlList[nHash]; pList = pList->Flink)
		{
			lpItem = CONTAINING_RECORD(pList,ADPT_PCRE_URL_CONTEXT2,ListEntry);
			lpMatch = &lpItem->Url;
			if( lpMatch->szData[0] == '\0' || lpMatch->usPrceLen == 0 ) //容错处理
				continue;

			if( lpMatch->ulHash == i && lpMatch->usPrceLen <= nUrlLen )
			{
				if( strstr(strUrl,lpMatch->szData) != NULL )
				{
					bRet = TRUE;
					*lpIdx = (ULONG)lpMatch->usHtmlIdx;
					break;
				}
			}
		}
		KeReleaseSpinLock(&pGlobal->csUrlLock,NewIrql);

		KmFree(strUrl);
	} while (FALSE);

	if( reply_buffer != NULL )
		KmFree(reply_buffer);

	return bRet;
}

NTSTATUS	tdx_send( PDEVICE_OBJECT dev, PIRP irp )
{
	PCHAR	in_data;
	INT		in_len;
	PGLOBAL_DATA	pGlobal = GetGlobal(NULL,NULL);
	PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation( irp ); 
	TDI_REQUEST_KERNEL_SEND *param = (TDI_REQUEST_KERNEL_SEND *)(&irpStack->Parameters);

	if( irp->MdlAddress )
	{
		in_data = tdi_get_databuffer( irp); 
		in_len = (INT)param->SendLength; 

		if( in_data != NULL && in_len > 0 )
		{
			ULONG ulIdx = 0xFFFFFFFF;
			if( MatchUrl(pGlobal,in_data,in_len,&ulIdx) )
			{
				//UpdateConnContext(pGlobal,irpStack->FileObject,1);
				NTSTATUS			status =  STATUS_SUCCESS;
				PADPT_CONN_CONTEXT  pEventHandle = NULL;
				PVOID				pConnCtx = NULL;
				ULONG				ulSize = 0;
				ULONG				ulBytesTaken = 0;
				PCHAR				szRedirectPage = NULL;
				PIRP				pIoRequstIrp = NULL;
				BOOLEAN				bRecvOk = FALSE;

				if( !GetConnContext(pGlobal,irpStack->FileObject,&pEventHandle) || pEventHandle == NULL || pEventHandle->EventRecviceHandler == NULL || pEventHandle->pConnCtx == NULL )
				{
					return tdx_call_lower_device( dev, irp );
				}

				if( !GetReplyContext(pGlobal,ulIdx,&szRedirectPage,&ulSize) || szRedirectPage == NULL || ulSize == 0 )
				{
					return tdx_call_lower_device( dev, irp );
				}

				irp->IoStatus.Status = STATUS_SUCCESS;
				irp->IoStatus.Information = (ULONG_PTR)in_len;
				IoCompleteRequest(irp,IO_NO_INCREMENT);

				pConnCtx = pEventHandle->pConnCtx;
				status = tdx_event_receive_old(pEventHandle,pConnCtx,0,ulSize,ulSize,&ulBytesTaken,szRedirectPage,&pIoRequstIrp);
				if( status == STATUS_MORE_PROCESSING_REQUIRED && pIoRequstIrp != NULL )
				{
					PMDL pMdl = pIoRequstIrp->MdlAddress;
					if( pMdl != NULL )
					{
						PCHAR pBuffer = (PCHAR)MmGetSystemAddressForMdlSafe(pMdl,NormalPagePriority);
						ULONG ulSize2 = MmGetMdlByteCount(pMdl);
						if( pBuffer != NULL && ulSize2 >= (ulSize - ulBytesTaken) )
						{
							RtlCopyMemory(pBuffer,&szRedirectPage[ulBytesTaken],ulSize-ulBytesTaken);

							pIoRequstIrp->IoStatus.Status = STATUS_SUCCESS;
							pIoRequstIrp->IoStatus.Information = ulSize-ulBytesTaken;
							IoCompleteRequest(pIoRequstIrp,IO_NO_INCREMENT);
							bRecvOk = TRUE;
						}
						//else
						//	return tdx_call_lower_device( dev, irp );
					}
				}
				else if( STATUS_SUCCESS == status && ulSize == ulBytesTaken )
				{
					bRecvOk = TRUE;
				}

				KmFree(szRedirectPage);
				return STATUS_SUCCESS;
			}
		}
	}

	return tdx_call_lower_device( dev, irp );
}


NTSTATUS tdx_dispatch( PDEVICE_OBJECT dev, PIRP Irp )
{
	NTSTATUS status = STATUS_SUCCESS; 
	PIO_STACK_LOCATION irpStack ;

	irpStack = IoGetCurrentIrpStackLocation( Irp ); 

	switch( irpStack->MajorFunction )
	{
	case IRP_MJ_CREATE:
		return tdx_create( dev, Irp ); 
		break;
	case IRP_MJ_CLEANUP:
		{
			tdx_cleanup(dev,Irp);
		}
		break;
	case IRP_MJ_DEVICE_CONTROL:
		if( KeGetCurrentIrql()==PASSIVE_LEVEL ){
			status = TdiMapUserRequest( dev, Irp, irpStack ); 
		}else{
			status = STATUS_NOT_IMPLEMENTED; 
		}
		if( !NT_SUCCESS(status)) break; //没映射成功，直接下发
		//如果成功则进入 IRP_MJ_INTERNAL_DEVICE_CONTROL
	case IRP_MJ_INTERNAL_DEVICE_CONTROL:
		{
			switch(irpStack->MinorFunction)
			{
			case TDI_ASSOCIATE_ADDRESS:
				{
					tdx_associate_address(dev,Irp);
					break;
				}
			case TDI_SET_EVENT_HANDLER:
				{
					tdx_set_handler(dev,Irp);
					break;
				}
			case TDI_SEND:
				{
					return tdx_send(dev,Irp);
				}
			default:
				break;
			}
		}
		break;
	default:
		break;
	}

	return tdx_call_lower_device(dev,Irp);
}

NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject, IN PUNICODE_STRING  RegistryPath)
{
	UNICODE_STRING DeviceName,Win32Device;
	PDEVICE_OBJECT DeviceObject = NULL;
	NTSTATUS status;
	unsigned i;
	PGLOBAL_DATA pGlobal = ::GetGlobal(DriverObject,DeviceObject);

	KdBreakPoint();
	RtlInitUnicodeString(&DeviceName,L"\\Device\\RedXx0");
	RtlInitUnicodeString(&Win32Device,L"\\DosDevices\\RedXx0");

	for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
		DriverObject->MajorFunction[i] = RedXxDefaultHandler;

	DriverObject->MajorFunction[IRP_MJ_CREATE] = RedXxCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = RedXxCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = RedXxDeviceControl;

	//DriverObject->DriverUnload = RedXxUnload;
	status = IoCreateDevice(DriverObject,
							0,
							&DeviceName,
							FILE_DEVICE_UNKNOWN,
							0,
							FALSE,
							&DeviceObject);
	if (!NT_SUCCESS(status))
		return status;
	if (!DeviceObject)
		return STATUS_UNEXPECTED_IO_ERROR;

	pGlobal->CtlDevice = DeviceObject;
	DeviceObject->Flags |= DO_DIRECT_IO;
	DeviceObject->AlignmentRequirement = FILE_WORD_ALIGNMENT;
	status = IoCreateSymbolicLink(&Win32Device, &DeviceName);

	if( !NT_SUCCESS(tdx_create_device( pGlobal , L"\\Device\\Tcp",TRUE) ) )
	{
		IoDeleteSymbolicLink(&Win32Device);
		IoDeleteDevice(DeviceObject);

		return STATUS_UNSUCCESSFUL;
	}

	DeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	return STATUS_SUCCESS;
}

void RedXxUnload(IN PDRIVER_OBJECT DriverObject)
{
// 	UNICODE_STRING Win32Device;
// 	RtlInitUnicodeString(&Win32Device,L"\\DosDevices\\RedXx0");
// 	IoDeleteSymbolicLink(&Win32Device);
// 	IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS RedXxCreateClose(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	PGLOBAL_DATA pGlobal = GetGlobal(NULL,NULL);

	if( DeviceObject != pGlobal->CtlDevice )
	{ 
		return tdx_dispatch(DeviceObject,Irp);
	}

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return STATUS_SUCCESS;
}

NTSTATUS RedXxDefaultHandler(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	PGLOBAL_DATA pGlobal = GetGlobal(NULL,NULL);

	if( DeviceObject != pGlobal->CtlDevice )
	{ 
		return tdx_dispatch(DeviceObject,Irp);
	}

	Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return Irp->IoStatus.Status;
}

void ChrLower(PCHAR pChr)
{
	CHAR Chr = *pChr;
	if( Chr >= 'A' && Chr <= 'Z' )
	{
		*pChr = 'a' + (Chr - 'A');
	}
}

void StrLower(PCHAR pStr,INT nLen)
{
	INT i;
	for(i=0; i < nLen; i++)
	{
		ChrLower(pStr+i);
	}
}

ULONG32 CaclUrlHashX(PCHAR szHost)
{
	char *	ptr;
	int		l;
	CHAR	szUrl[4096] = {0};

	l = strlen(szHost);
	RtlCopyMemory(szUrl,(LPCSTR)szHost,l + 1);
	_strlwr((char*)szUrl);
	ptr = strchr(szUrl,'/');
	if( ptr != NULL )
	{
		*ptr = '\0';
	}

	l = strlen(szUrl);

	ULONG32 ulSum = CaclStringHashX(szUrl,l);
	if( ptr != NULL )
		*ptr = '/';

	return ulSum;
}

BOOLEAN AddCfgUrl(PGLOBAL_DATA pGlobal,ULONG ulGroupId,PCHAR pStart,PCHAR pEnd)
{
	PCHAR			pPtr;
	PADPT_PCRE_URL_CONTEXT2 pUrlX;
	PADPT_PCRE_URL2	pUrl;
	INT				nLen;

	if( pStart == pEnd )	return FALSE;

	while( (*(pEnd - 1)) == ' ' )
	{
		pEnd --;
		*pEnd = '\0';
	}

	while( (*(pStart)) == ' ' ) pStart++;

	nLen = (INT)(pEnd - pStart);
	if( nLen <= 0 )	return FALSE;

	StrLower(pStart,nLen);
	pUrlX = (PADPT_PCRE_URL_CONTEXT2)KmAlloc(sizeof(LIST_ENTRY)+ sizeof(ADPT_PCRE_URL2) + nLen + 1);
	if( !pUrlX )	return FALSE;

	pUrl = &pUrlX->Url;

	pUrl->usHtmlIdx = (USHORT)ulGroupId;
	pUrl->ulHash = CaclUrlHashX(pStart);
	pUrl->usPrceLen = (USHORT)nLen;
	pUrl->usTotalLength = (USHORT)sizeof(ADPT_PCRE_URL2) + pUrl->usPrceLen;
	RtlCopyMemory(pUrl->szData,(LPCSTR)pStart,nLen+1);
	pUrl->szData[nLen+1] = '\0';

	ExInterlockedInsertTailList(&pGlobal->UrlList[ pUrl->ulHash % MAX_HASH_NUM ],&pUrlX->ListEntry,&pGlobal->csUrlLock);
	return FALSE;
}

BOOLEAN LoadDataFromFile(LPCWSTR lpFile,PCHAR * pData,INT * nSize)
{
	HANDLE				hFile;
	NTSTATUS			status;
	OBJECT_ATTRIBUTES	Oa;
	UNICODE_STRING		ufile_name;
	PCHAR				lpBuf = NULL;
	INT					nMax = 0;
	int					nLen = 0;
	IO_STATUS_BLOCK		io_status = {0};
	LARGE_INTEGER		offset = {0};
	FILE_STANDARD_INFORMATION file_standard_info = { 0 };

	if( pData == NULL || lpFile == NULL || nSize == NULL )
		return FALSE;

	*pData = NULL;
	*nSize = 0;

	RtlInitUnicodeString(&ufile_name,(PCWSTR)lpFile);
	InitializeObjectAttributes(&Oa,&ufile_name,OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,NULL,NULL);

	status = ZwCreateFile(&hFile,GENERIC_READ|GENERIC_WRITE,&Oa,&io_status,NULL,FILE_ATTRIBUTE_NORMAL,FILE_SHARE_READ,
		FILE_OPEN,FILE_NON_DIRECTORY_FILE|FILE_RANDOM_ACCESS|FILE_SYNCHRONOUS_IO_NONALERT,NULL,0);

	if( !NT_SUCCESS(status) )
	{
		return FALSE;
	}

	status = ZwQueryInformationFile(
		hFile,
		&io_status,
		&file_standard_info,
		sizeof(file_standard_info),
		FileStandardInformation);

	if( !NT_SUCCESS(status) )
	{
		ZwClose(hFile);
		return FALSE;
	}

	nMax = (INT)file_standard_info.EndOfFile.QuadPart;
	lpBuf = (PCHAR)KmAlloc(nMax+2);	
	if( !lpBuf )
	{
		ZwClose(hFile);
		return FALSE;
	}

	nLen = 0;
	while(nLen<nMax)
	{
		io_status.Information = 0;
		status = ZwReadFile(hFile,NULL,NULL,NULL,&io_status,lpBuf+nLen,nMax-nLen,&offset,NULL);
		if( !NT_SUCCESS(status) )
		{
			ZwClose(hFile);
			KmFree(lpBuf);
			return FALSE;
		}

		nLen += (int)io_status.Information;
		offset.QuadPart = nLen;
	}

	ZwClose(hFile);
	if( nLen != nMax )
	{
		KmFree(lpBuf);
		return FALSE;
	}

	lpBuf[nLen] = '\0';
	lpBuf[nLen+1] = '\0';

	*pData = lpBuf;
	*nSize = nLen;

	return TRUE;
}

#define  strJs_1 "HTTP/1.1 302 Found\r\nLocation: %s\r\nContent-Type:text/html; charset=utf-8\r\nContent-Length: 39\r\nConnection: close\r\n\r\n<html><head></head><body></body></html>..."

void AddGroupRule(PGLOBAL_DATA pGlobal,ULONG ulGroupId,LPWSTR pFileName,PCHAR strTarget,PCHAR strUrl)
{
	PCHAR			pStr,pStart,pEnd;
	INT				nSize = 0;
	INT				i,m,n;
	LPWSTR			pFile;
	PTG_HTML		pHtml;

	if( !pFileName || !pGlobal )
		return ;

	m = wcslen(pFileName);
	n = strlen(strUrl);
	i = m + n + 6;

	i <<= 1;
	pFile = (LPWSTR)KmAlloc(i);
	if( !pFile )
	{
		return ;
	}

	RtlCopyMemory(pFile,pFileName,m + m);
	for(i = 0; i < n; i++)
	{
		pFile[m +i] = (WCHAR)strUrl[i];
	}

	pFile[m +n] = L'\0';
	if( !LoadDataFromFile(pFile,&pStr,&nSize) )
	{
		KmFree(pFile);
		return;
	}

	pHtml = (PTG_HTML)KmAlloc(sizeof(strJs_1) + strlen(strTarget) + sizeof(TG_HTML));
	if( !pHtml )
	{
		KmFree(pFile);
		return;
	}

	pHtml->ulIdx = ulGroupId;
	pHtml->strTarget = (PCHAR)&pHtml[1];
	pHtml->ulLen = sprintf(pHtml->strTarget,strJs_1,strTarget);
	ExInterlockedInsertTailList(&pGlobal->TargetList,&pHtml->ListEntry,&pGlobal->csTargetLock);


	pStart = pStr;
	while (pStart != NULL)
	{
		pEnd = strchr(pStart,';');
		if( pEnd == NULL )
		{
			pEnd = pStart + strlen(pStart);
			AddCfgUrl(pGlobal,ulGroupId,pStart,pEnd);
			break;
		}

		*pEnd = '\0';
		AddCfgUrl(pGlobal,ulGroupId,pStart,pEnd);
		pStart = pEnd + 1;
	}

	if( pStr != NULL )
	{
		KmFree(pStr);
		pStr = NULL;
	}

	KmFree(pFile);
}


void* cfg_read(PGLOBAL_DATA pGlobal,LPWSTR pFileName,LPWSTR pFile)
{
	PCHAR			pStr,pStart,pEnd;
	INT				nSize = 0;
	INT				i,nCntMin = 0,nCntMax = 0;
	AD_MEM_INI		Ini = {0};

	if( !pFile || !pFileName || !pGlobal )
		return NULL;

	if( !MemIni_LoadDataFromFile(&Ini,pFile) )
	{
		return NULL;
	}

	MemIni_ReadInteger(&Ini,"main","CntMin",&nCntMin);
	MemIni_ReadInteger(&Ini,"main","CntMax",&nCntMax);

	if( nCntMax <= 0 || nCntMin <= 0 )
		return NULL;

	for(i=nCntMin;i <= nCntMax; i++)
	{
		CHAR strNo[50] = {0};
		INT nSwitch = 0;
		PCHAR	strTarget = NULL;
		INT		nTarget = 0;
		PCHAR	strUrl = NULL;
		INT		nUrl = 0;

		sprintf(strNo,"%d",i);
		MemIni_ReadInteger(&Ini,strNo,"switch",&nSwitch);
		if( nSwitch != 1 )
			continue;

		MemIni_ReadString(&Ini,strNo,"target",&strTarget,&nTarget);
		MemIni_ReadString(&Ini,strNo,"url",&strUrl,&nUrl);

		if( !strTarget || nTarget <= 0 )
			continue;

		if( !strUrl || nUrl <= 0 )
		{
			ExFreePool(strTarget);
			continue;
		}

		AddGroupRule(pGlobal,(USHORT)i,pFileName,strTarget,strUrl);
		ExFreePool(strTarget);
		ExFreePool(strUrl);
	}

	return NULL;
}

void* cfg_thread(void *arg)
{
	LPWSTR			pFileName = (LPWSTR)arg;
	PGLOBAL_DATA	pGlobal = GetGlobal(NULL,NULL);
	INT				i,m,n;
	LPWSTR			pFile;

	if( !pFileName || !pGlobal )
		return NULL;

	m = wcslen(pFileName);
	n = wcslen(L"config.dat");
	i = m + n + 6;

	i <<= 1;
	pFile = (LPWSTR)KmAlloc(i);
	if( !pFile )
	{
		return NULL;
	}

	RtlCopyMemory(pFile,pFileName,m + m);
	RtlCopyMemory(&pFile[m],L"config.dat",sizeof(L"config.dat"));

	cfg_read(pGlobal,pFileName,pFile);

	KmFree(pFile);

	return NULL;
}

NTSTATUS RedXxDeviceControl(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	NTSTATUS			status = STATUS_INVALID_PARAMETER;
	PIO_STACK_LOCATION	irpSp;
	PVOID				ioBuffer;
	ULONG				inputBufferLength, outputBufferLength;
	ULONG				ioControlCode;

	PGLOBAL_DATA pGlobal = GetGlobal(NULL,NULL);

	if( DeviceObject != pGlobal->CtlDevice )
	{ 
		return tdx_dispatch(DeviceObject,Irp);
	}

	irpSp = IoGetCurrentIrpStackLocation(Irp);
	Irp->IoStatus.Information = 0;

	ioBuffer			= Irp->AssociatedIrp.SystemBuffer;
	inputBufferLength	= irpSp->Parameters.DeviceIoControl.InputBufferLength;
	outputBufferLength	= irpSp->Parameters.DeviceIoControl.OutputBufferLength;
	ioControlCode		= irpSp->Parameters.DeviceIoControl.IoControlCode;

	switch (ioControlCode)
	{
	case IOCTL_DRIVERCODE_ADDCFG:
		{
			PWCHAR				strUrl;
			LPWSTR				pFileName;

			strUrl = (PWCHAR)ioBuffer;
			if( ioBuffer == NULL || inputBufferLength < sizeof(WCHAR) || strUrl[0] == L'\0')
				break;

			pFileName = (LPWSTR)KmAlloc(inputBufferLength + 2);
			if( pFileName == NULL )
			{
				status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}

			RtlCopyMemory(pFileName,strUrl,inputBufferLength + 2);
			pFileName[inputBufferLength>>1] = L'\0';
			status = Create_Thread(cfg_thread, pFileName);
		}
		break;
	case IOCTL_DRIVERCODE_CLRCFG:
		{
			PLIST_ENTRY			pList;
			LIST_ENTRY			leTmp;
			ULONG				i;
			PADPT_PCRE_URL_CONTEXT2	lpItem = NULL;
			KIRQL				NewIrql;
			PGLOBAL_DATA		pGlobal = GetGlobal(NULL,NULL);

			InitializeListHead(&leTmp);

			KeAcquireSpinLock(&pGlobal->csUrlLock,&NewIrql);
			for(i = 0; i < MAX_HASH_NUM; i++)
			{
				while(!IsListEmpty(&pGlobal->UrlList[i]))
				{
					lpItem = CONTAINING_RECORD(RemoveTailList(&pGlobal->UrlList[i]),ADPT_PCRE_URL_CONTEXT2,ListEntry);
					InsertHeadList(&leTmp,&lpItem->ListEntry);
				}
			}
			KeReleaseSpinLock(&pGlobal->csUrlLock,NewIrql);

			while(!IsListEmpty(&leTmp))
			{
				lpItem = CONTAINING_RECORD(RemoveTailList(&leTmp),ADPT_PCRE_URL_CONTEXT2,ListEntry);
				KmFree(lpItem);
			}

			KeAcquireSpinLock(&pGlobal->csTargetLock,&NewIrql);
			while(!IsListEmpty(&pGlobal->TargetList))
			{
				PTG_HTML lpItemX = CONTAINING_RECORD(RemoveTailList(&pGlobal->TargetList),TG_HTML,ListEntry);
				InsertHeadList(&leTmp,&lpItemX->ListEntry);
			}
			KeReleaseSpinLock(&pGlobal->csTargetLock,NewIrql);

			while(!IsListEmpty(&leTmp))
			{
				PTG_HTML lpItemX = CONTAINING_RECORD(RemoveTailList(&leTmp),TG_HTML,ListEntry);
				KmFree(lpItemX);
			}

			status = STATUS_SUCCESS;
		}
		break;

	default:
		KdPrint(("[DriverCode] IRP_MJ_DEVICE_CONTROL: IoControlCode = 0x%x (%04x,%04x)\n",
			ioControlCode, DEVICE_TYPE_FROM_CTL_CODE(ioControlCode),
			IoGetFunctionCodeFromCtlCode(ioControlCode)));

		break;
	}


	Irp->IoStatus.Status = status;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return status;
}