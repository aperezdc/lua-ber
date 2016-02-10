#ifndef BER_UTILS_H
#define BER_UTILS_H

int oid2str (lua_State *L);
int str2oid (lua_State *L);
int bitstr2num (lua_State *L);
int num2bitstr (lua_State *L);

#endif
