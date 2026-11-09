"""Small synchronous administrative test bus; no installed Aetheri API.

Only the explicit disposable-carrier audio fixture uses this module. Callers
own their pending-effect journal and trusted host PID1/bus admission. No call
retries, activation, reconnection, name replacement or outcome inference.
"""
import ctypes as c


class Error(c.Structure):
    _fields_=[('name',c.c_char_p),('message',c.c_char_p),('need_free',c.c_int)]


class Bus:
    def __init__(self,private=False):
        self.private=private
        self.lib=c.CDLL('libsystemd.so.0');self.bus=c.c_void_p()
        signatures={
            'sd_bus_open_system':([c.POINTER(c.c_void_p)],c.c_int),
            'sd_bus_new':([c.POINTER(c.c_void_p)],c.c_int),
            'sd_bus_set_address':([c.c_void_p,c.c_char_p],c.c_int),
            'sd_bus_negotiate_fds':([c.c_void_p,c.c_int],c.c_int),
            'sd_bus_start':([c.c_void_p],c.c_int),
            'sd_bus_unref':([c.c_void_p],c.c_void_p),
            'sd_bus_set_method_call_timeout':([c.c_void_p,c.c_uint64],c.c_int),
            'sd_bus_message_new_method_call':([c.c_void_p,c.POINTER(c.c_void_p),c.c_char_p,c.c_char_p,c.c_char_p,c.c_char_p],c.c_int),
            'sd_bus_message_unref':([c.c_void_p],c.c_void_p),
            'sd_bus_message_append_basic':([c.c_void_p,c.c_char,c.c_void_p],c.c_int),
            'sd_bus_message_read_basic':([c.c_void_p,c.c_char,c.c_void_p],c.c_int),
            'sd_bus_message_has_signature':([c.c_void_p,c.c_char_p],c.c_int),
            'sd_bus_message_open_container':([c.c_void_p,c.c_char,c.c_char_p],c.c_int),
            'sd_bus_message_close_container':([c.c_void_p],c.c_int),
            'sd_bus_call':([c.c_void_p,c.c_void_p,c.c_uint64,c.c_void_p,c.POINTER(c.c_void_p)],c.c_int),
            'sd_bus_error_free':([c.POINTER(Error)],None),
        }
        for name,(arguments,result) in signatures.items():
            fn=getattr(self.lib,name);fn.argtypes=arguments;fn.restype=result
        try:
            if private:
                self.check(self.lib.sd_bus_new(c.byref(self.bus)))
                self.check(self.lib.sd_bus_set_address(self.bus,b'unix:path=/run/systemd/private'))
                self.check(self.lib.sd_bus_negotiate_fds(self.bus,1))
                self.check(self.lib.sd_bus_start(self.bus))
            else:self.check(self.lib.sd_bus_open_system(c.byref(self.bus)))
            self.check(self.lib.sd_bus_set_method_call_timeout(self.bus,3000000))
        except BaseException:
            self.close();raise

    @staticmethod
    def check(result):
        if result<0:raise OSError(-result,'administrative bus call failed; do not replay')
        return result

    def append(self,message,kind,value):
        if kind in 'so':data=c.c_char_p(value.encode())
        else:data={'b':c.c_int,'u':c.c_uint32,'i':c.c_int32,'t':c.c_uint64,'h':c.c_int}[kind](value)
        self.check(self.lib.sd_bus_message_append_basic(message,kind.encode(),c.cast(data,c.c_void_p) if kind in 'so' else c.byref(data)))

    def open(self,message,kind,signature):self.check(self.lib.sd_bus_message_open_container(message,kind.encode(),signature.encode()))
    def end(self,message):self.check(self.lib.sd_bus_message_close_container(message))

    def value(self,message,signature,value):
        if len(signature)==1 and signature in 'sobuith':self.append(message,signature,value)
        elif signature=='as':
            self.open(message,'a','s')
            for item in value:self.append(message,'s',item)
            self.end(message)
        elif signature=='ai':
            self.open(message,'a','i')
            for item in value:self.append(message,'i',item)
            self.end(message)
        elif signature=='(bs)':
            self.open(message,'r','bs');self.append(message,'b',value[0]);self.append(message,'s',value[1]);self.end(message)
        elif signature=='a(hs)':
            self.open(message,'a','(hs)')
            for fd,name in value:
                self.open(message,'r','hs');self.append(message,'h',fd);self.append(message,'s',name);self.end(message)
            self.end(message)
        elif signature=='a(sasb)':
            self.open(message,'a','(sasb)')
            for executable,arguments,ignore in value:
                self.open(message,'r','sasb');self.append(message,'s',executable);self.value(message,'as',arguments);self.append(message,'b',ignore);self.end(message)
            self.end(message)
        else:raise ValueError('unsupported administrative property signature')

    def call(self,member,build=None,destination='org.freedesktop.systemd1',path='/org/freedesktop/systemd1',interface='org.freedesktop.systemd1.Manager',result=None):
        request=c.c_void_p();reply=c.c_void_p();error=Error()
        try:
            target=None if self.private and destination=='org.freedesktop.systemd1' else destination.encode()
            self.check(self.lib.sd_bus_message_new_method_call(self.bus,c.byref(request),target,path.encode(),interface.encode(),member.encode()))
            if build:build(request)
            status=self.lib.sd_bus_call(self.bus,request,3000000,c.byref(error),c.byref(reply))
            if status<0:
                detail=': '.join(raw.decode(errors='replace') for raw in (error.name,error.message) if raw)
                raise OSError(-status,'administrative bus call failed; do not replay: '+detail)
            if self.lib.sd_bus_message_has_signature(reply,(result or '').encode())!=1:raise ValueError('different administrative reply signature; do not replay')
            if result:
                value=c.c_char_p() if result in 'so' else c.c_uint32()
                if self.check(self.lib.sd_bus_message_read_basic(reply,result.encode(),c.byref(value)))!=1:raise ValueError('missing administrative reply')
                return value.value.decode() if result in 'so' else value.value
        finally:
            self.lib.sd_bus_error_free(c.byref(error))
            self.lib.sd_bus_message_unref(reply);self.lib.sd_bus_message_unref(request)

    def create(self,name,properties):
        def build(message):
            self.append(message,'s',name);self.append(message,'s','fail');self.open(message,'a','(sv)')
            for key,signature,value in properties:
                self.open(message,'r','sv');self.append(message,'s',key);self.open(message,'v',signature)
                self.value(message,signature,value);self.end(message);self.end(message)
            self.end(message);self.open(message,'a','(sa(sv))');self.end(message)
        return self.call('StartTransientUnit',build,result='o')

    def unit(self,member,name):return self.call(member,lambda m:self.append(m,'s',name))
    def stop(self,name):return self.call('StopUnit',lambda m:(self.append(m,'s',name),self.append(m,'s','fail')),result='o')
    def close(self):
        if self.bus:self.lib.sd_bus_unref(self.bus);self.bus=c.c_void_p()
