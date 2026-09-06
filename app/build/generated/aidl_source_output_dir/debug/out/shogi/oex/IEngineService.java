/*
 * This file is auto-generated.  DO NOT MODIFY.
 * Using: C:\\Users\\Admin\\AppData\\Local\\Android\\Sdk\\build-tools\\36.0.0\\aidl.exe -pC:\\Users\\Admin\\AppData\\Local\\Android\\Sdk\\platforms\\android-35\\framework.aidl -oC:\\VS\\Workspace\\atshogi-android\\app\\build\\generated\\aidl_source_output_dir\\debug\\out -IC:\\VS\\Workspace\\atshogi-android\\app\\src\\main\\aidl -IC:\\VS\\Workspace\\atshogi-android\\app\\src\\debug\\aidl -IC:\\Users\\Admin\\.gradle\\caches\\9.5.0\\transforms\\a199d1a1a0c85759fd23f5b64279975f\\transformed\\core-1.12.0\\aidl -IC:\\Users\\Admin\\.gradle\\caches\\9.5.0\\transforms\\10e7c48ca3cbdfd1bd07ea11c2e8989a\\transformed\\versionedparcelable-1.1.1\\aidl -dC:\\Users\\Admin\\AppData\\Local\\Temp\\aidl8072628859166943916.d C:\\VS\\Workspace\\atshogi-android\\app\\src\\main\\aidl\\shogi\\oex\\IEngineService.aidl
 *
 * DO NOT CHECK THIS FILE INTO A CODE TREE (e.g. git, etc..).
 * ALWAYS GENERATE THIS FILE FROM UPDATED AIDL COMPILER
 * AS A BUILD INTERMEDIATE ONLY. THIS IS NOT SOURCE CODE.
 */
package shogi.oex;
public interface IEngineService extends android.os.IInterface
{
  /** Default implementation for IEngineService. */
  public static class Default implements shogi.oex.IEngineService
  {
    @Override public void registerCallback(shogi.oex.IEngineServiceCallback callback) throws android.os.RemoteException
    {
    }
    @Override public void unregisterCallback(shogi.oex.IEngineServiceCallback callback) throws android.os.RemoteException
    {
    }
    @Override public void writeCommand(java.lang.String cmd) throws android.os.RemoteException
    {
    }
    // ShogiHome dynamically calls getInfo
    @Override public java.lang.String getInfo() throws android.os.RemoteException
    {
      return null;
    }
    @Override
    public android.os.IBinder asBinder() {
      return null;
    }
  }
  /** Local-side IPC implementation stub class. */
  public static abstract class Stub extends android.os.Binder implements shogi.oex.IEngineService
  {
    /** Construct the stub and attach it to the interface. */
    @SuppressWarnings("this-escape")
    public Stub()
    {
      this.attachInterface(this, DESCRIPTOR);
    }
    /**
     * Cast an IBinder object into an shogi.oex.IEngineService interface,
     * generating a proxy if needed.
     */
    public static shogi.oex.IEngineService asInterface(android.os.IBinder obj)
    {
      if ((obj==null)) {
        return null;
      }
      android.os.IInterface iin = obj.queryLocalInterface(DESCRIPTOR);
      if (((iin!=null)&&(iin instanceof shogi.oex.IEngineService))) {
        return ((shogi.oex.IEngineService)iin);
      }
      return new shogi.oex.IEngineService.Stub.Proxy(obj);
    }
    @Override public android.os.IBinder asBinder()
    {
      return this;
    }
    @Override public boolean onTransact(int code, android.os.Parcel data, android.os.Parcel reply, int flags) throws android.os.RemoteException
    {
      java.lang.String descriptor = DESCRIPTOR;
      if (code >= android.os.IBinder.FIRST_CALL_TRANSACTION && code <= android.os.IBinder.LAST_CALL_TRANSACTION) {
        data.enforceInterface(descriptor);
      }
      if (code == INTERFACE_TRANSACTION) {
        reply.writeString(descriptor);
        return true;
      }
      switch (code)
      {
        case TRANSACTION_registerCallback:
        {
          shogi.oex.IEngineServiceCallback _arg0;
          _arg0 = shogi.oex.IEngineServiceCallback.Stub.asInterface(data.readStrongBinder());
          this.registerCallback(_arg0);
          reply.writeNoException();
          break;
        }
        case TRANSACTION_unregisterCallback:
        {
          shogi.oex.IEngineServiceCallback _arg0;
          _arg0 = shogi.oex.IEngineServiceCallback.Stub.asInterface(data.readStrongBinder());
          this.unregisterCallback(_arg0);
          reply.writeNoException();
          break;
        }
        case TRANSACTION_writeCommand:
        {
          java.lang.String _arg0;
          _arg0 = data.readString();
          this.writeCommand(_arg0);
          reply.writeNoException();
          break;
        }
        case TRANSACTION_getInfo:
        {
          java.lang.String _result = this.getInfo();
          reply.writeNoException();
          reply.writeString(_result);
          break;
        }
        default:
        {
          return super.onTransact(code, data, reply, flags);
        }
      }
      return true;
    }
    private static class Proxy implements shogi.oex.IEngineService
    {
      private android.os.IBinder mRemote;
      Proxy(android.os.IBinder remote)
      {
        mRemote = remote;
      }
      @Override public android.os.IBinder asBinder()
      {
        return mRemote;
      }
      public java.lang.String getInterfaceDescriptor()
      {
        return DESCRIPTOR;
      }
      @Override public void registerCallback(shogi.oex.IEngineServiceCallback callback) throws android.os.RemoteException
      {
        android.os.Parcel _data = android.os.Parcel.obtain();
        android.os.Parcel _reply = android.os.Parcel.obtain();
        try {
          _data.writeInterfaceToken(DESCRIPTOR);
          _data.writeStrongInterface(callback);
          boolean _status = mRemote.transact(Stub.TRANSACTION_registerCallback, _data, _reply, 0);
          _reply.readException();
        }
        finally {
          _reply.recycle();
          _data.recycle();
        }
      }
      @Override public void unregisterCallback(shogi.oex.IEngineServiceCallback callback) throws android.os.RemoteException
      {
        android.os.Parcel _data = android.os.Parcel.obtain();
        android.os.Parcel _reply = android.os.Parcel.obtain();
        try {
          _data.writeInterfaceToken(DESCRIPTOR);
          _data.writeStrongInterface(callback);
          boolean _status = mRemote.transact(Stub.TRANSACTION_unregisterCallback, _data, _reply, 0);
          _reply.readException();
        }
        finally {
          _reply.recycle();
          _data.recycle();
        }
      }
      @Override public void writeCommand(java.lang.String cmd) throws android.os.RemoteException
      {
        android.os.Parcel _data = android.os.Parcel.obtain();
        android.os.Parcel _reply = android.os.Parcel.obtain();
        try {
          _data.writeInterfaceToken(DESCRIPTOR);
          _data.writeString(cmd);
          boolean _status = mRemote.transact(Stub.TRANSACTION_writeCommand, _data, _reply, 0);
          _reply.readException();
        }
        finally {
          _reply.recycle();
          _data.recycle();
        }
      }
      // ShogiHome dynamically calls getInfo
      @Override public java.lang.String getInfo() throws android.os.RemoteException
      {
        android.os.Parcel _data = android.os.Parcel.obtain();
        android.os.Parcel _reply = android.os.Parcel.obtain();
        java.lang.String _result;
        try {
          _data.writeInterfaceToken(DESCRIPTOR);
          boolean _status = mRemote.transact(Stub.TRANSACTION_getInfo, _data, _reply, 0);
          _reply.readException();
          _result = _reply.readString();
        }
        finally {
          _reply.recycle();
          _data.recycle();
        }
        return _result;
      }
    }
    static final int TRANSACTION_registerCallback = (android.os.IBinder.FIRST_CALL_TRANSACTION + 0);
    static final int TRANSACTION_unregisterCallback = (android.os.IBinder.FIRST_CALL_TRANSACTION + 1);
    static final int TRANSACTION_writeCommand = (android.os.IBinder.FIRST_CALL_TRANSACTION + 2);
    static final int TRANSACTION_getInfo = (android.os.IBinder.FIRST_CALL_TRANSACTION + 3);
  }
  /** @hide */
  public static final java.lang.String DESCRIPTOR = "shogi.oex.IEngineService";
  public void registerCallback(shogi.oex.IEngineServiceCallback callback) throws android.os.RemoteException;
  public void unregisterCallback(shogi.oex.IEngineServiceCallback callback) throws android.os.RemoteException;
  public void writeCommand(java.lang.String cmd) throws android.os.RemoteException;
  // ShogiHome dynamically calls getInfo
  public java.lang.String getInfo() throws android.os.RemoteException;
}
