/*
 *  Resql
 *
 *  Copyright (C) 2021 Resql Authors
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "meta.h"
#include "rs.h"

#include "sc/sc_array.h"
#include "sc/sc_str.h"
#include "sc/sc_uri.h"
#include "sqlite/sqlite3.h"

#include <string.h>

const char* meta_role_str [] = {
        "leader",
        "follower",
        "joined"
};

void meta_node_init(struct meta_node *n, struct sc_uri *uri)
{
    n->name = sc_str_create(uri->userinfo);

    sc_array_create(n->uris, 2);
    sc_array_add(n->uris, uri);

    n->connected = false;
    n->role = META_FOLLOWER;
}

void meta_node_term(struct meta_node *n)
{
    struct sc_uri *uri;

    sc_str_destroy(n->name);

    sc_array_foreach (n->uris, uri) {
        sc_uri_destroy(uri);
    }
    sc_array_destroy(n->uris);
}

void meta_node_encode(struct meta_node *n, struct sc_buf *buf)
{
    struct sc_uri *uri;

    sc_buf_put_str(buf, n->name);
    sc_buf_put_bool(buf, n->connected);
    sc_buf_put_8(buf, n->role);
    sc_buf_put_32(buf, sc_array_size(n->uris));

    sc_array_foreach (n->uris, uri) {
        sc_buf_put_str(buf, uri->str);
    }
}

void meta_node_decode(struct meta_node *n, struct sc_buf *buf)
{
    uint32_t count;
    struct sc_uri *uri;

    n->name = sc_str_create(sc_buf_get_str(buf));
    n->connected = sc_buf_get_8(buf);
    n->role = sc_buf_get_8(buf);

    sc_array_create(n->uris, 2);

    count = sc_buf_get_32(buf);

    for (int i = 0; i < count; i++) {
        uri = sc_uri_create(sc_buf_get_str(buf));
        if (!uri) {
            rs_abort("meta");
        }

        sc_array_add(n->uris, uri);
    }
}

void meta_init(struct meta *m, const char *cluster_name)
{
    *m = (struct meta){0};

    m->name = sc_str_create(cluster_name);
    m->uris = sc_str_create("");
    
    sc_array_create(m->nodes, 3);
}

void meta_term(struct meta *m)
{
    struct meta_node node;

    sc_str_destroy(m->name);
    sc_str_destroy(m->uris);

    sc_array_foreach (m->nodes, node) {
        meta_node_term(&node);
    }
    sc_array_destroy(m->nodes);

    if (m->prev) {
        meta_term(m->prev);
        rs_free(m->prev);
    }
}

void meta_node_copy(struct meta_node *n, struct meta_node *src)
{
    struct sc_uri *uri;

    n->name = sc_str_create(src->name);
    n->connected = src->connected;
    n->role = src->role;

    sc_array_create(n->uris, 2);

    sc_array_foreach (src->uris, uri) {
        sc_array_add(n->uris, sc_uri_create(uri->str));
    }
}

void meta_copy(struct meta *m, struct meta *src)
{
    struct meta_node n, other;

    meta_term(m);
    meta_init(m, src->name);

    sc_str_set(&m->uris, src->uris);
    m->term = src->term;
    m->index = src->index;
    m->voter = src->voter;

    sc_array_foreach (src->nodes, other) {
        meta_node_copy(&n, &other);
        sc_array_add(m->nodes, n);
    }

    if (src->prev != NULL) {
        m->prev = rs_malloc(sizeof(*m->prev));
        meta_init(m->prev, src->prev->name);
        meta_copy(m->prev, src->prev);
    }
}

void meta_encode(struct meta *m, struct sc_buf *buf)
{
    struct meta_node n;

    sc_buf_put_str(buf, m->name);
    sc_buf_put_str(buf, m->uris);
    sc_buf_put_64(buf, m->term);
    sc_buf_put_64(buf, m->index);
    sc_buf_put_32(buf, m->voter);
    sc_buf_put_32(buf, sc_array_size(m->nodes));

    sc_array_foreach (m->nodes, n) {
        meta_node_encode(&n, buf);
    }

    if (!m->prev) {
        sc_buf_put_bool(buf, false);
        return;
    }

    sc_buf_put_bool(buf, true);
    meta_encode(m->prev, buf);
}

void meta_decode(struct meta *m, struct sc_buf *buf)
{
    uint32_t count;
    struct meta_node n;

    sc_str_set(&m->name, sc_buf_get_str(buf));
    sc_str_set(&m->uris, sc_buf_get_str(buf));
    m->term = sc_buf_get_64(buf);
    m->index = sc_buf_get_64(buf);
    m->voter = sc_buf_get_32(buf);

    count = sc_buf_get_32(buf);
    for (int i = 0; i < count; i++) {
        meta_node_decode(&n, buf);
        sc_array_add(m->nodes, n);
    }

    if (sc_buf_get_bool(buf)) {
        m->prev = rs_malloc(sizeof(*m->prev));
        meta_init(m->prev, m->name);
        meta_decode(m->prev, buf);
    }
}

static void meta_update(struct meta *m)
{
    uint32_t voter = 0;
    struct sc_buf tmp;

    for (int i = 0; i<  sc_array_size(m->nodes); i++) {
        if (m->nodes[i].role != META_JOINED) {
            voter++;
        }
    }

    m->voter = voter;

    sc_buf_init(&tmp, 1024);

    for (int i = 0; i<  sc_array_size(m->nodes); i++) {
        if (m->nodes[i].role == META_LEADER) {
            for (int j = 0; j < sc_array_size(m->nodes[i].uris); j++) {
                sc_buf_put_text(&tmp, "%s ", m->nodes[i].uris[j]->str);
            }
            break;
        }
    }

    for (int i = 0; i<  sc_array_size(m->nodes); i++) {
        if (m->nodes[i].role != META_LEADER) {
            for (int j = 0; j < sc_array_size(m->nodes[i].uris); j++) {
                sc_buf_put_text(&tmp, "%s ", m->nodes[i].uris[j]->str);
            }
        }
    }

    sc_str_set(&m->uris, sc_buf_rbuf(&tmp));
    sc_buf_term(&tmp);
}

bool meta_add(struct meta *m, struct sc_uri *uri)
{
    struct meta_node node;
    struct meta* tmp;

    assert(m->prev == NULL);

    for (int i = 0; i < sc_array_size(m->nodes); i++) {
        if (strcmp(m->nodes[i].name, uri->userinfo) == 0) {
            return false;
        }
    }

    tmp = rs_malloc(sizeof(*tmp));
    meta_init(tmp, "");
    meta_copy(tmp, m);
    m->prev = tmp;

    meta_node_init(&node, sc_uri_create(uri->str));
    node.role = META_JOINED;
    sc_array_add(m->nodes, node);
    meta_update(m);

    return true;
}

bool meta_remove(struct meta *m, const char* name)
{
    bool found = false;
    struct meta* tmp;

    assert(m->prev == NULL);

    for (int i = 0; i < sc_array_size(m->nodes); i++) {
        if (strcmp(m->nodes[i].name, name) == 0) {
            found = true;
            break;
        }
    }

    if (!found) {
        return false;
    }


    tmp = rs_malloc(sizeof(*tmp));
    meta_init(tmp, "");
    meta_copy(tmp, m);
    m->prev = tmp;

    for (int i = 0; i < sc_array_size(m->nodes); i++) {
        if (strcmp(m->nodes[i].name,name) == 0) {
            sc_array_del(m->nodes, i);
            break;
        }
    }

    meta_update(m);

    return true;
}

bool meta_exists(struct meta *m, const char* name)
{
    for (int i = 0; i < sc_array_size(m->nodes); i++) {
        if (strcmp(m->nodes[i].name, name) == 0) {
            return true;
        }
    }

    return false;
}

void meta_remove_prev(struct meta* m)
{
    assert(m->prev != NULL);

    meta_term(m->prev);
    rs_free(m->prev);
    m->prev = NULL;
}

void meta_set_connected(struct meta *m, const char *name)
{
    for (int i = 0; i < sc_array_size(m->nodes); i++) {
        if (strcmp(m->nodes[i].name, name) == 0) {
            m->nodes[i].connected = true;
            break;
        }
    }
}

void meta_set_disconnected(struct meta* m, const char* name)
{
    for (int i = 0; i < sc_array_size(m->nodes); i++) {
        if (strcmp(m->nodes[i].name, name) == 0) {
            m->nodes[i].connected = false;
            break;
        }
    }
}

void meta_clear_connection(struct meta *m)
{
    for (int i = 0; i < sc_array_size(m->nodes); i++) {
        m->nodes[i].connected = false;
    }
}

void meta_set_leader(struct meta *m, const char *name)
{
    bool found;

    for (int i = 0; i < sc_array_size(m->nodes); i++) {
        if (strcmp(m->nodes[i].name, name) == 0) {
            m->nodes[i].role = META_LEADER;
            found = true;
            continue;
        }

        if (m->nodes[i].role == META_LEADER) {
            m->nodes[i].role = META_FOLLOWER;
        }
    }

    assert(found);
    meta_update(m);
}

int meta_parse_uris(struct meta *m, char *addrs)
{
    bool found;
    int rc = RS_ERROR;
    char *dup, *save = NULL;
    const char *tok;
    struct meta_node n;
    struct sc_uri *uri;

    dup = sc_str_create(addrs);
    if (dup == NULL) {
        return RS_ERROR;
    }

    while ((tok = sc_str_token_begin(dup, &save, " ")) != NULL) {
        if (*tok == '\0') {
            break;
        }

        uri = sc_uri_create(tok);
        if (uri == NULL || *uri->userinfo == '\0') {
            goto clean_uri;
        }

        found = false;

        for (int i = 0; i < sc_array_size(m->nodes); i++) {
            if (strcmp(uri->userinfo, m->nodes[i].name) == 0) {
                sc_array_add(m->nodes[i].uris, uri);
                found = true;
                break;
            }
        }

        if (!found) {
            meta_node_init(&n, uri);
            sc_array_add(m->nodes, n);
        }
    }

    meta_update(m);
    rc = RS_OK;

clean_uri:
    sc_str_destroy(dup);

    if (rc == RS_ERROR) {
        sc_str_destroy(m->name);
        sc_str_destroy(m->uris);
    }

    return rc;
}

void meta_print(struct meta *m, struct sc_buf *buf)
{
    sc_buf_put_text(buf, "\n| -------------------------------\n");
    sc_buf_put_text(buf, "| Cluster : %s \n", m->name);
    sc_buf_put_text(buf, "| Term    : %lu \n", (unsigned long) m->term);

    for (int i = 0; i < sc_array_size(m->nodes); i++) {
        sc_buf_put_text(buf, "| Node    : %s, Role : %s \n", m->nodes[i].name,
                        meta_role_str[m->nodes[i].role]);
    }
    sc_buf_put_text(buf, "| -------------------------------\n");
}
