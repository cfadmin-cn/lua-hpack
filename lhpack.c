#define LUA_LIB

#include <core.h>
#include <nghttp2/nghttp2.h>

#define MAX_TABLE (512)

struct hpack_ctx_t {
  size_t tsize; size_t closed;
  nghttp2_hd_deflater *encoder;
  nghttp2_hd_inflater *decoder;
};

static int hpack_encode(lua_State *L) {
  struct hpack_ctx_t *hctx = luaL_checkudata(L, 1, "__HPACK__");
  if (!hctx)
    return luaL_error(L, "HPACK encode: Invalid string buffer.");

  nghttp2_nv nList[MAX_TABLE];

  int index = 0;
  lua_pushnil(L);
  while (lua_next(L, 2)) {
    size_t namelen, valuelen;
    const char* name = lua_tolstring(L, -2, &namelen);
    const char* value = lua_tolstring(L, -1, &valuelen);
    nList[index] = (nghttp2_nv){
      .name = (uint8_t*)name,    .namelen = namelen,
      .value = (uint8_t*)value,  .valuelen = valuelen,
      .flags = NGHTTP2_NV_FLAG_NO_COPY_NAME | NGHTTP2_NV_FLAG_NO_COPY_VALUE,
    };
    lua_pop(L, 1);
    index++;
  }

  luaL_Buffer B;
  char *buf = luaL_buffinitsize(L, &B, nghttp2_hd_deflate_bound(hctx->encoder, nList, index));

  int len = nghttp2_hd_deflate_hd(hctx->encoder, (uint8_t *)buf, bsize, nList, index);
  if (len < 0)
    return luaL_error(L, "HPACK encode failed with error: %d, %s", ret, nghttp2_strerror(len));

  luaL_pushresultsize(&B, len);

  return 1;
}

static int hpack_decode(lua_State *L) {
  struct hpack_ctx_t *hctx = luaL_checkudata(L, 1, "__HPACK__");
  size_t bsize = 0;
  const char * buf = luaL_checklstring(L, 2, &bsize);
  if (!buf || bsize < 1)
    return luaL_error(L, "HPACK decode: Invalid string buffer.");

  lua_createtable(L, 32, 0);

  for (;;) {

    int inflate_flags = 0;

    nghttp2_nv nv = {};

    int dsize = nghttp2_hd_inflate_hd2(hctx->decoder, &nv, &inflate_flags, (uint8_t*)buf, bsize, 1);
    if (dsize < 0) {
      lua_pushnil(L);
      lua_pushfstring(L, "HPACK decode failed with error: %s", nghttp2_strerror(dsize));
      return 2;
    }

    /* 每次更新buffer位置 */
    buf += dsize; bsize -= dsize;

    if (inflate_flags & NGHTTP2_HD_INFLATE_FINAL){
      nghttp2_hd_inflate_end_headers(hctx->decoder);
      break;
    }

    if (inflate_flags & NGHTTP2_HD_INFLATE_EMIT) {
      lua_pushlstring(L, (const char*)nv.name, nv.namelen);
      lua_pushlstring(L, (const char*)nv.value, nv.valuelen);
      lua_rawset(L, -3);
    }

  }

  return 1;
}

static int hpack_gc(lua_State *L) {
  struct hpack_ctx_t *hctx = luaL_checkudata(L, 1, "__HPACK__");
  if (hctx->closed)
    return 1;
  hctx->closed = 1;
  if (hctx->encoder){
    nghttp2_hd_deflate_del(hctx->encoder);
    hctx->encoder = NULL;
  }
  if (hctx->decoder){
    nghttp2_hd_inflate_del(hctx->decoder);
    hctx->decoder = NULL;
  }
  return 1;
}

static int hpack_new(lua_State *L) {
  struct hpack_ctx_t *hctx = lua_newuserdata(L, sizeof(struct hpack_ctx_t));
  if (!hctx) return 0;

  hctx->tsize = luaL_checkinteger(L, 2);
  if (hctx->tsize <= 0)
    hctx->tsize = 4096;

  hctx->closed = 0;

  /* 初始化编码解码器 */
  int ret1, ret2;
  if ((ret1 = nghttp2_hd_deflate_new(&hctx->encoder, hctx->tsize)))
    return luaL_error(L, "HPACK internal error: %s", nghttp2_strerror(ret1));

  if ((ret2 = nghttp2_hd_inflate_new(&hctx->decoder)))
    return luaL_error(L, "HPACK internal error: %s", nghttp2_strerror(ret2));

  luaL_setmetatable(L, "__HPACK__");

  return 1;
}

static inline void hpack_init(lua_State *L) {
  luaL_newmetatable(L, "__HPACK__");
  /* index meta table */
  lua_pushstring (L, "__index");
  lua_pushvalue(L, -2);
  lua_rawset(L, -3);
  /* week table */
  lua_pushliteral(L, "__mode");
  lua_pushliteral(L, "kv");
  lua_rawset(L, -3);

  /* base method */
  luaL_Reg hpack_libs[] ={
    {"encode", hpack_encode},
    {"decode", hpack_decode},
    {"__gc",   hpack_gc},
    {NULL, NULL}
  };
  luaL_setfuncs(L, hpack_libs, 0);

  lua_newtable(L);
  /* new class */
  lua_pushliteral(L, "new");
  lua_pushcfunction(L, hpack_new);
  lua_rawset(L, -3);
  /* set version */
  lua_pushliteral(L, "http2_version");
  lua_pushliteral(L, NGHTTP2_VERSION);
  lua_rawset(L, -3);
}

LUAMOD_API int luaopen_lhpack(lua_State *L) {
  luaL_checkversion(L);
  hpack_init(L);
  return 1;
}
