from _libomrx_cffi import ffi, lib

# Import all the constants/etc from lib into this namespace
for k, v in lib.__dict__.items():
    if k.startswith('OMRX_'):
        globals()[k] = v
del k
del v

@ffi.def_extern()
def _log_warning(omrx, errcode, msg):
    msg = ffi.string(msg)
    msg = ffi.string(msg)
    if omrx != ffi.NULL:
        omrx = ffi.from_handle(lib.omrx_user_data(omrx))
    else:
        omrx = Omrx._current_instance
    if omrx:
        omrx.log_warning(errcode, msg)
    else:
        # This shouldn't happen, but just in case, at least print the message
        #FIXME: use warnings or logging module
        sys.stderr.write("libomrx warning: {}\n".format(msg))

@ffi.def_extern()
def _log_error(omrx, errcode, msg):
    msg = ffi.string(msg)
    if omrx != ffi.NULL:
        omrx = ffi.from_handle(lib.omrx_user_data(omrx))
    else:
        omrx = Omrx._current_instance
    if omrx:
        try:
            omrx.log_error(errcode, msg)
        except Exception, e:
            omrx._exception = e
    else:
        # This shouldn't happen, but just in case, at least print the message
        #FIXME: use logging module
        sys.stderr.write("libomrx error: {}\n".format(msg))

if lib.omrx_init() != lib.OMRX_OK:
    raise ImportError("omrx_init() failed")

class OmrxError (Exception):
    def __init__(self, errcode, msg):
        self.errcode = errcode
        self.msg = msg

    def __str__(self):
        return self.msg

class OmrxOSError (OmrxError):
    pass

class AllocError (OmrxError):
    pass

class EOFError (OmrxError):
    pass

class NotOpenError (OmrxError):
    pass

class AlreadyOpenError (OmrxError):
    pass

class BadMagicError (OmrxError):
    pass

class BadVersionError (OmrxError):
    pass

class BadChunkError (OmrxError):
    pass

class WrongDtypeError (OmrxError):
    pass

class InternalError (OmrxError):
    pass


_error_classes = {
    OMRX_ERR_OSERR: OmrxOSError,
    OMRX_ERR_ALLOC: AllocError,
    OMRX_ERR_EOF: EOFError,
    OMRX_ERR_NOT_OPEN: NotOpenError,
    OMRX_ERR_ALREADY_OPEN: AlreadyOpenError,
    OMRX_ERR_BAD_MAGIC: BadMagicError,
    OMRX_ERR_BAD_VER: BadVersionError,
    OMRX_ERR_BAD_CHUNK: BadChunkError,
    OMRX_ERR_WRONG_DTYPE: WrongDtypeError,
    OMRX_ERR_INTERNAL: InternalError,
}

def omrx_exception(errcode, msg):
    cls = _error_classes.get(errcode, OmrxError)
    return cls(errcode, msg)

def open(filename):
    return Omrx().open(filename)

class Omrx:
    _current_instance = None

    def __init__(self):
        omrx_p = ffi.new('omrx_t *')
        self._exception = None
        self._handle = ffi.new_handle(self)
        #FIXME: make this threadsafe
        Omrx._current_instance = self
        lib.omrx_new(self._handle, omrx_p)
        Omrx._current_instance = None
        self.check_error()
        self.omrx = omrx_p[0]

    def log_error(self, errcode, msg):
        raise omrx_exception(errcode, msg)

    def log_warning(self, errcode, msg):
        #FIXME: use warnings or logging module
        sys.stderr.write("libomrx warning: {}\n".format(msg))

    def check_error(self):
        e = self._exception
        if e:
            self._exception = None
            raise e

    def open(self, filename):
        #TODO: file pointer?
        lib.omrx_open(self.omrx, filename, ffi.NULL)
        self.check_error()
        return self

    def get_chunk_by_id(self, id, tag=None):
        chunk_p = ffi.new('omrx_chunk_t *')
        if not tag:
            tag = ffi.NULL
        lib.omrx_get_chunk_by_id(self.omrx, id, tag, chunk_p)
        self.check_error()
        return Chunk(self, chunk_p[0])

    def __getitem__(self, item):
        return self.get_chunk_by_id(item)

    def __del__(self):
        lib.omrx_free(self.omrx)
        # Destructors shouldn't raise exceptions, so we can't check_error() here
        #FIXME: maybe print an error message or something on failure

class Chunk:
    _ffi_types = { 
        OMRX_DTYPE_U8: "uint8_t",
        OMRX_DTYPE_S8: "int8_t",
        OMRX_DTYPE_U16: "uint16_t",
        OMRX_DTYPE_S16: "int16_t",
        OMRX_DTYPE_U32: "uint32_t",
        OMRX_DTYPE_S32: "int32_t",
        OMRX_DTYPE_F32: "float",
        OMRX_DTYPE_U64: "uint64_t",
        OMRX_DTYPE_S64: "int64_t",
        OMRX_DTYPE_F64: "double",
    }

    def __init__(self, omrx, chunk_ptr):
        self.omrx = omrx
        self.chunk = chunk_ptr

    def get_child(self, tag):
        chunk_p = ffi.new('omrx_chunk_t *')
        lib.omrx_get_child(self.chunk, tag, chunk_p)
        self.omrx.check_error()
        return Chunk(self.omrx, chunk_p[0])

    def get_attr_info(self, id):
        info_p = ffi.new('struct omrx_attr_info *')
        lib.omrx_get_attr_info(self.chunk, id, info_p)
        self.omrx.check_error()
        if not info_p[0].exists:
            raise KeyError(id)
        return info_p[0]

    def get_attr(self, id):
        data_p = ffi.new('void **')
        info = self.get_attr_info(id)
        lib.omrx_get_attr_raw(self.chunk, id, ffi.NULL, data_p)
        self.omrx.check_error()
        return self._cast_to_dtype(info, data_p[0])

    def __getitem__(self, item):
        return self.get_attr(item)

    def _cast_to_dtype(self, info, data):
        if info.raw_type == OMRX_DTYPE_RAW:
            return ffi.buffer(data, info.size)
        if info.raw_type == OMRX_DTYPE_UTF8:
            data = ffi.cast("char[{}]".format(info.size), data)
            return ffi.string(data)
        ffi_type = self._ffi_types.get(info.elem_type)
        if ffi_type:
            if info.is_array:
                ffi_type = "{}[{}][{}]".format(ffi_type, info.rows, info.cols)
                return ffi.cast(ffi_type, data)
            else:
                ffi_type += " *"
                return ffi.cast(ffi_type, data)[0]
        raise ValueError("Unknown element type: {:04x}".format(info.elem_type))


