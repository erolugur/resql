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

#include "test_util.h"

#include "c/resql.h"
#include "sc/sc_uri.h"

#include <server.h>
#include <settings.h>
#include <stdio.h>
#include <unistd.h>

struct server *create_node_0()
{
    char *options[] = {"", "-e"};

    struct settings settings;

    settings_init(&settings);
    settings_read_cmdline(&settings, sizeof(options) / sizeof(char *), options);

    sc_str_set(&settings.node.log_level, "DEBUG");
    sc_str_set(&settings.node.name, "node0");
    sc_str_set(&settings.node.bind_uri,
               "tcp://node0@127.0.0.1:7600 unix:///tmp/var0");
    sc_str_set(&settings.node.ad_uri, "tcp://node0@127.0.0.1:7600");
    sc_str_set(
            &settings.cluster.nodes,
            "tcp://node0@127.0.0.1:7600 tcp://node1@127.0.0.1:7601 tcp://node2@127.0.0.1:7602");
    sc_str_set(&settings.node.dir, "/tmp/node0");
    settings.node.in_memory = true;

    struct server *server = server_create(&settings);

    int rc = server_start(server, true);
    if (rc != RS_OK) {
        abort();
    }

    return server;
}

struct server *create_node_0_existing()
{
    char *options[] = {""};

    struct settings settings;

    settings_init(&settings);
    settings_read_cmdline(&settings, sizeof(options) / sizeof(char *), options);

    sc_str_set(&settings.node.log_level, "DEBUG");
    sc_str_set(&settings.node.name, "node0");
    sc_str_set(&settings.node.bind_uri,
               "tcp://node0@127.0.0.1:7600 unix:///tmp/var0");
    sc_str_set(&settings.node.ad_uri, "tcp://node0@127.0.0.1:7600");
    sc_str_set(
            &settings.cluster.nodes,
            "tcp://node0@127.0.0.1:7600 tcp://node1@127.0.0.1:7601 tcp://node2@127.0.0.1:7602");
    sc_str_set(&settings.node.dir, "/tmp/node0");
    settings.node.in_memory = true;

    struct server *server = server_create(&settings);

    int rc = server_start(server, true);
    if (rc != RS_OK) {
        abort();
    }

    return server;
}

struct server *create_node_1()
{
    char *options[] = {"", "-e"};

    struct settings settings;

    settings_init(&settings);
    settings_read_cmdline(&settings, sizeof(options) / sizeof(char *), options);

    sc_str_set(&settings.node.log_level, "DEBUG");
    sc_str_set(&settings.node.name, "node1");
    sc_str_set(&settings.node.bind_uri,
               "tcp://node1@127.0.0.1:7601 unix:///tmp/var1");
    sc_str_set(&settings.node.ad_uri, "tcp://node1@127.0.0.1:7601");
    sc_str_set(
            &settings.cluster.nodes,
            "tcp://node0@127.0.0.1:7600 tcp://node1@127.0.0.1:7601 tcp://node2@127.0.0.1:7602");
    sc_str_set(&settings.node.dir, "/tmp/node1");
    settings.node.in_memory = true;

    struct server *server = server_create(&settings);

    int rc = server_start(server, true);
    if (rc != RS_OK) {
        abort();
    }

    return server;
}

struct server *create_node_2()
{
    char *options[] = {"", "-e"};

    struct settings settings;

    settings_init(&settings);
    settings_read_cmdline(&settings, sizeof(options) / sizeof(char *), options);

    sc_str_set(&settings.node.log_level, "DEBUG");
    sc_str_set(&settings.node.name, "node2");
    sc_str_set(&settings.node.bind_uri,
               "tcp://node2@127.0.0.1:7602 unix:///tmp/var2");
    sc_str_set(&settings.node.ad_uri, "tcp://node2@127.0.0.1:7602");
    sc_str_set(
            &settings.cluster.nodes,
            "tcp://node0@127.0.0.1:7600 tcp://node1@127.0.0.1:7601 tcp://node2@127.0.0.1:7602");
    sc_str_set(&settings.node.dir, "/tmp/node2");
    settings.node.in_memory = true;

    struct server *server = server_create(&settings);

    int rc = server_start(server, true);
    if (rc != RS_OK) {
        abort();
    }

    return server;
}

void test()
{
    int rc;
    resql *c;
    resql_result *rs;
    struct resql_column *row;
    const char *uris =
            "tcp://127.0.0.1:7600 tcp://127.0.0.1:7601 tcp://127.0.0.1:7602";

    struct server *s0, *s1;

    s0 = create_node_0();
    s1 = create_node_1();


    struct resql_config config = {.cluster_name = "cluster",
                                      .client_name = "any",
                                      .timeout = 10000,
                                      .uris = uris};

    resql_create(&c, &config);

    resql_put_sql(c, "SELECT 'multi';");
    rc = resql_exec(c, true, &rs);
    if (rc == RESQL_OK) {
        while ((row = resql_row(rs)) != NULL) {
            for (int i = 0; i < resql_column_count(rs); i++) {
                printf("%s \n", row[i].name);
                printf("%s \n", row[i].text);
            }
        }
    }

    for (int i = 0; i < 1000; i++) {
        resql_put_sql(c, "SELECT 'multi';");
        rc = resql_exec(c, false, &rs);
        if (rc == RESQL_OK) {
            while ((row = resql_row(rs)) != NULL) {
                for (int j = 0; j < resql_column_count(rs); j++) {
                    printf("%s \n", row[j].name);
                    printf("%s \n", row[j].text);
                }
            }
        }

        sleep(1);
    }

    server_stop(s0);
    server_stop(s1);
}

void write_test()
{
    int rc;
    resql *c;
    resql_result *rs;
    const char *uris =
            "tcp://127.0.0.1:7600 tcp://127.0.0.1:7601 tcp://127.0.0.1:7602";

    struct server *s0, *s1;

    s0 = create_node_0();
    s1 = create_node_1();


    struct resql_config config = {.cluster_name = "cluster",
                                      .client_name = "any",
                                      .timeout = 30000,
                                      .uris = uris};

    resql_create(&c, &config);

    resql_put_sql(c, "CREATE TABLE snapshot (key TEXT, value TEXT);");

    rc = resql_exec(c, false, &rs);
    if (rc != RESQL_OK) {
        rs_abort("");
    }

    for (int i = 0; i < 1000; i++) {
        for (int j = 0; j < 1000; j++) {
            char tmp[32];
            snprintf(tmp, sizeof(tmp), "%d", (i * 1000) + j);

            resql_put_sql(c, "INSERT INTO snapshot VALUES(:key, 'value')");
            resql_bind_param(c, ":key", "%s", tmp);
        }

        rc = resql_exec(c, false, &rs);
        if (rc != RESQL_OK) {
            rs_abort("");
        }
    }

    sleep(100000000);

    server_stop(s0);
    server_stop(s1);
}

void write_test2()
{
    int rc;
    resql *c;
    resql_result *rs;
    const char *uris =
            "tcp://127.0.0.1:7600 tcp://127.0.0.1:7601 tcp://127.0.0.1:7602";

    struct server *s0, *s1, *s2;

    s0 = create_node_0();
    s1 = create_node_1();

    struct resql_config config = {.cluster_name = "cluster",
                                      .client_name = "any",
                                      .timeout = 10000,
                                      .uris = uris};

    resql_create(&c, &config);

    resql_put_sql(c, "CREATE TABLE snapshot (key TEXT, value TEXT);");

    rc = resql_exec(c, false, &rs);
    if (rc != RESQL_OK) {
        rs_abort("");
    }

    for (int i = 0; i < 1000; i++) {
        for (int j = 0; j < 1000; j++) {
            char tmp[32];
            snprintf(tmp, sizeof(tmp), "%d", (i * 1000) + j);

            resql_put_sql(c, "INSERT INTO snapshot VALUES(:key, 'value')");
            resql_bind_param(c, ":key", "%s", tmp);
        }

        rc = resql_exec(c, false, &rs);
        if (rc != RESQL_OK) {
            rs_abort("");
        }
    }

    server_stop(s0);
    server_stop(s1);

    s0 = create_node_0_existing();
    s2 = create_node_2();

    sleep(100000000);

    server_stop(s0);
    server_stop(s1);
    server_stop(s2);
}

int main()
{
    // test_execute(test);
    // test_execute(write_test);
    test_execute(write_test2);
}
