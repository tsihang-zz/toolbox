/* Copyright (C) 2007-2010 Open Information Security Foundation
 *
 * You can copy, redistribute or modify this Program under the terms of
 * the GNU General Public License version 2 as published by the Free
 * Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/**
 * \file
 *
 * \author Endace Technology Limited - Jason Ish <jason.ish@endace.com>
 *
 * This file provides a basic configuration system for the IDPS
 * engine.
 *
 * NOTE: Setting values should only be done from one thread during
 * engine initialization.  Multiple threads should be able access read
 * configuration data.  Allowing run time changes to the configuration
 * will require some locks.
 *
 * \todo Consider having the in-memory configuration database a direct
 *   reflection of the configuration file and moving command line
 *   parameters to a primary lookup table?
 *
 * \todo Get rid of allow override and go with a simpler first set,
 *   stays approach?
 */

#include "oryx.h"
#include "conf.h"

/** Maximum size of a complete domain name. */
#define NODE_NAME_MAX 1024

static ConfNode *root = NULL;
static ConfNode *root_backup = NULL;

/**
 * \brief Helper function to get a node, creating it if it does not
 * exist.
 *
 * This function exits on memory failure as creating configuration
 * nodes is usually part of application initialization.
 *
 * \param name The name of the configuration node to get.
 * \param final Flag to set created nodes as final or not.
 *
 * \retval The existing configuration node if it exists, or a newly
 *   created node for the provided name.  On error, NULL will be returned.
 */
static ConfNode *ConfGetNodeOrCreate(const char *name, int final)
{
    ConfNode *parent = root;
    ConfNode *node = NULL;
    char node_name[NODE_NAME_MAX];
    char *key;
    char *next;

    if (strlcpy(node_name, name, sizeof(node_name)) >= sizeof(node_name)) {
        fprintf (stdout, 
            "Configuration name too long: %s", name);
        return NULL;
    }

    key = node_name;

    do {
        if ((next = strchr(key, '.')) != NULL)
            *next++ = '\0';
        if ((node = ConfNodeLookupChild(parent, key)) == NULL) {
            node = ConfNodeNew();
            if (unlikely(node == NULL)) {
                fprintf (stdout, 
                    "Failed to allocate memory for configuration.");
                goto end;
            }
            node->name = strdup(key);
            if (unlikely(node->name == NULL)) {
                ConfNodeFree(node);
                node = NULL;
                fprintf (stdout, 
                    "Failed to allocate memory for configuration.");
                goto end;
            }
            node->parent = parent;
            node->final = final;
            TAILQ_INSERT_TAIL(&parent->head, node, next);
        }
        key = next;
        parent = node;
    } while (next != NULL);

end:
    return node;
}

/**
 * \brief Initialize the configuration system.
 */
void ConfInit(void)
{
    if (root != NULL) {
        fprintf (stdout, "already initialized\n");
        return;
    }
    root = ConfNodeNew();
    if (root == NULL) {
        fprintf (stdout, 
            "ERROR: Failed to allocate memory for root configuration node, "
            "aborting.\n");
        exit(EXIT_FAILURE);
    }
    fprintf (stdout, "configuration module initialized\n");
}

/**
 * \brief Allocate a new configuration node.
 *
 * \retval An allocated configuration node on success, NULL on failure.
 */
ConfNode *ConfNodeNew(void)
{
    ConfNode *new;

    new = calloc(1, sizeof(*new));
    if (unlikely(new == NULL)) {
        return NULL;
    }
    TAILQ_INIT(&new->head);

    return new;
}

/**
 * \brief Free a ConfNode and all of its children.
 *
 * \param node The configuration node to free.
 */
void ConfNodeFree(ConfNode *node)
{
    ConfNode *tmp;

    while ((tmp = TAILQ_FIRST(&node->head))) {
        TAILQ_REMOVE(&node->head, tmp, next);
        ConfNodeFree(tmp);
    }

    if (node->name != NULL)
        free(node->name);
    if (node->val != NULL)
        free(node->val);
    free(node);
}

/**
 * \brief Get a ConfNode by name.
 *
 * \param name The full name of the configuration node to lookup.
 *
 * \retval A pointer to ConfNode is found or NULL if the configuration
 *    node does not exist.
 */
ConfNode *ConfGetNode(const char *name)
{
    ConfNode *node = root;
    char node_name[NODE_NAME_MAX];
    char *key;
    char *next;

    if (strlcpy(node_name, name, sizeof(node_name)) >= sizeof(node_name)) {
        fprintf (stdout, 
            "Configuration name too long: %s", name);
        return NULL;
    }

    key = node_name;
    do {
        if ((next = strchr(key, '.')) != NULL)
            *next++ = '\0';
        node = ConfNodeLookupChild(node, key);
        key = next;
    } while (next != NULL && node != NULL);

    return node;
}

/**
 * \brief Get the root configuration node.
 */
ConfNode *ConfGetRootNode(void)
{
    return root;
}

/**
 * \brief Set a configuration value.
 *
 * Configuration values set with this function may be overridden by
 * subsequent calls, or if the value appears multiple times in a
 * configuration file.
 *
 * \param name The name of the configuration parameter to set.
 * \param val The value of the configuration parameter.
 *
 * \retval 1 if the value was set otherwise 0.
 */
int ConfSet(const char *name, const char *val)
{
    ConfNode *node = ConfGetNodeOrCreate(name, 0);
    if (node == NULL || node->final) {
        return 0;
    }
    if (node->val != NULL)
        free(node->val);
    node->val = strdup(val);
    if (unlikely(node->val == NULL)) {
        return 0;
    }
    return 1;
}

/**
 * \brief Set a configuration parameter from a string.
 *
 * Where the input string is something like:
 *    stream.midstream=true
 *
 * \param input the input string to be parsed.
 *
 * \retval 1 if the value of set, otherwise 0.
 */
int ConfSetFromString(const char *input, int final)
{
    int retval = 0;
    char *name = strdup(input), *val = NULL;
    if (unlikely(name == NULL)) {
        goto done;
    }
    val = strchr(name, '=');
    if (val == NULL) {
        goto done;
    }
    *val++ = '\0';

    while (isspace((int)name[strlen(name) - 1])) {
        name[strlen(name) - 1] = '\0';
    }

    while (isspace((int)*val)) {
        val++;
    }

    if (final) {
        if (!ConfSetFinal(name, val)) {
            goto done;
        }
    }
    else {
        if (!ConfSet(name, val)) {
            goto done;
        }
    }

    retval = 1;
done:
    if (name != NULL) {
        free(name);
    }
    return retval;
}

/**
 * \brief Set a final configuration value.
 *
 * A final configuration value is a value that cannot be overridden by
 * the configuration file.  Its mainly useful for setting values that
 * are supplied on the command line prior to the configuration file
 * being loaded.  However, a subsequent call to this function can
 * override a previously set value.
 *
 * \param name The name of the configuration parameter to set.
 * \param val The value of the configuration parameter.
 *
 * \retval 1 if the value was set otherwise 0.
 */
int ConfSetFinal(const char *name, const char *val)
{
    ConfNode *node = ConfGetNodeOrCreate(name, 1);
    if (node == NULL) {
        return 0;
    }
    if (node->val != NULL)
        free(node->val);
    node->val = strdup(val);
    if (unlikely(node->val == NULL)) {
        return 0;
    }
    node->final = 1;
    return 1;
}

/**
 * \brief Retrieve the value of a configuration node.
 *
 * This function will return the value for a configuration node based
 * on the full name of the node.  It is possible that the value
 * returned could be NULL, this could happen if the requested node
 * does exist but is not a node that contains a value, but contains
 * children ConfNodes instead.
 *
 * \param name Name of configuration parameter to get.
 * \param vptr Pointer that will be set to the configuration value parameter.
 *   Note that this is just a reference to the actual value, not a copy.
 *
 * \retval 1 will be returned if the name is found, otherwise 0 will
 *   be returned.
 */
int ConfGet(const char *name, const char **vptr)
{
    ConfNode *node = ConfGetNode(name);
    if (node == NULL) {
        fprintf (stdout, "failed to lookup configuration parameter '%s'", name);
        return 0;
    }
    else {
        *vptr = node->val;
        return 1;
    }
}

/**
 * \brief Retrieve the value of a configuration node.
 *
 * This function will return the value for a configuration node based
 * on the full name of the node. This function notifies if vptr returns NULL
 * or if name is set to NULL.
 *
 * \param name Name of configuration parameter to get.
 * \param vptr Pointer that will be set to the configuration value parameter.
 *   Note that this is just a reference to the actual value, not a copy.
 *
 * \retval 0 will be returned if name was not found,
 *    1 will be returned if the name and it's value was found,
 *   -1 if the value returns NULL,
 *   -2 if name is NULL.
 */
int ConfGetValue(const char *name, const char **vptr)
{
    ConfNode *node;

    if (name == NULL) {
        fprintf (stdout, 
			"parameter 'name' is NULL");
        return -2;
    }

    node = ConfGetNode(name);

    if (node == NULL) {
        fprintf (stdout, "failed to lookup configuration parameter '%s'", name);
        return 0;
    }
    else {

        if (node->val == NULL) {
            fprintf (stdout, "value for configuration parameter '%s' is NULL", name);
            return -1;
        }

        *vptr = node->val;
        return 1;
    }

}

int ConfGetChildValue(const ConfNode *base, const char *name, const char **vptr)
{
    ConfNode *node = ConfNodeLookupChild(base, name);

    if (node == NULL) {
        fprintf (stdout, "failed to lookup configuration parameter '%s'", name);
        return 0;
    }
    else {
        *vptr = node->val;
        return 1;
    }
}


int ConfGetChildValueWithDefault(const ConfNode *base, const ConfNode *dflt,
    const char *name, const char **vptr)
{
    int ret = ConfGetChildValue(base, name, vptr);
    /* Get 'default' value */
    if (ret == 0 && dflt) {
        return ConfGetChildValue(dflt, name, vptr);
    }
    return ret;
}

/**
 * \brief Retrieve a configuration value as an integer.
 *
 * \param name Name of configuration parameter to get.
 * \param val Pointer to an intmax_t that will be set the
 * configuration value.
 *
 * \retval 1 will be returned if the name is found and was properly
 * converted to an interger, otherwise 0 will be returned.
 */
int ConfGetInt(const char *name, intmax_t *val)
{
    const char *strval = NULL;
    intmax_t tmpint;
    char *endptr;

    if (ConfGet(name, &strval) == 0)
        return 0;

    if (strval == NULL) {
        fprintf (stdout, 
			"malformed integer value "
                "for %s: NULL", name);
        return 0;
    }

    errno = 0;
    tmpint = strtoimax(strval, &endptr, 0);
    if (strval[0] == '\0' || *endptr != '\0') {
        fprintf (stdout, 
			"malformed integer value "
                "for %s: '%s'", name, strval);
        return 0;
    }
    if (errno == ERANGE && (tmpint == INTMAX_MAX || tmpint == INTMAX_MIN)) {
        fprintf (stdout, 
			"integer value for %s out "
                "of range: '%s'", name, strval);
        return 0;
    }

    *val = tmpint;
    return 1;
}

int ConfGetChildValueInt(const ConfNode *base, const char *name, intmax_t *val)
{
    const char *strval = NULL;
    intmax_t tmpint;
    char *endptr;

    if (ConfGetChildValue(base, name, &strval) == 0)
        return 0;
    errno = 0;
    tmpint = strtoimax(strval, &endptr, 0);
    if (strval[0] == '\0' || *endptr != '\0') {
        fprintf (stdout, 
			"malformed integer value "
                "for %s with base %s: '%s'", name, base->name, strval);
        return 0;
    }
    if (errno == ERANGE && (tmpint == INTMAX_MAX || tmpint == INTMAX_MIN)) {
        fprintf (stdout, 
			"integer value for %s with "
                " base %s out of range: '%s'", name, base->name, strval);
        return 0;
    }

    *val = tmpint;
    return 1;

}

int ConfGetChildValueIntWithDefault(const ConfNode *base, const ConfNode *dflt,
    const char *name, intmax_t *val)
{
    int ret = ConfGetChildValueInt(base, name, val);
    /* Get 'default' value */
    if (ret == 0 && dflt) {
        return ConfGetChildValueInt(dflt, name, val);
    }
    return ret;
}


/**
 * \brief Retrieve a configuration value as an boolen.
 *
 * \param name Name of configuration parameter to get.
 * \param val Pointer to an int that will be set to 1 for true, or 0
 * for false.
 *
 * \retval 1 will be returned if the name is found and was properly
 * converted to a boolean, otherwise 0 will be returned.
 */
int ConfGetBool(const char *name, int *val)
{
    const char *strval = NULL;

    *val = 0;
    if (ConfGetValue(name, &strval) != 1)
        return 0;

    *val = ConfValIsTrue(strval);

    return 1;
}

int ConfGetChildValueBool(const ConfNode *base, const char *name, int *val)
{
    const char *strval = NULL;

    *val = 0;
    if (ConfGetChildValue(base, name, &strval) == 0)
        return 0;

    *val = ConfValIsTrue(strval);

    return 1;
}

int ConfGetChildValueBoolWithDefault(const ConfNode *base, const ConfNode *dflt,
    const char *name, int *val)
{
    int ret = ConfGetChildValueBool(base, name, val);
    /* Get 'default' value */
    if (ret == 0 && dflt) {
        return ConfGetChildValueBool(dflt, name, val);
    }
    return ret;
}


/**
 * \brief Check if a value is true.
 *
 * The value is considered true if it is a string with the value of 1,
 * yes, true or on.  The test is not case sensitive, any other value
 * is false.
 *
 * \param val The string to test for a true value.
 *
 * \retval 1 If the value is true, 0 if not.
 */
int ConfValIsTrue(const char *val)
{
    const char *trues[] = {"1", "yes", "true", "on"};
    size_t u;

    for (u = 0; u < sizeof(trues) / sizeof(trues[0]); u++) {
        if (strcasecmp(val, trues[u]) == 0) {
            return 1;
        }
    }

    return 0;
}

/**
 * \brief Check if a value is false.
 *
 * The value is considered false if it is a string with the value of 0,
 * no, false or off.  The test is not case sensitive, any other value
 * is not false.
 *
 * \param val The string to test for a false value.
 *
 * \retval 1 If the value is false, 0 if not.
 */
int ConfValIsFalse(const char *val)
{
    const char *falses[] = {"0", "no", "false", "off"};
    size_t u;

    for (u = 0; u < sizeof(falses) / sizeof(falses[0]); u++) {
        if (strcasecmp(val, falses[u]) == 0) {
            return 1;
        }
    }

    return 0;
}

/**
 * \brief Retrieve a configuration value as a double
 *
 * \param name Name of configuration parameter to get.
 * \param val Pointer to an double that will be set the
 * configuration value.
 *
 * \retval 1 will be returned if the name is found and was properly
 * converted to a double, otherwise 0 will be returned.
 */
int ConfGetDouble(const char *name, double *val)
{
    const char *strval = NULL;
    double tmpdo;
    char *endptr;

    if (ConfGet(name, &strval) == 0)
        return 0;

    errno = 0;
    tmpdo = strtod(strval, &endptr);
    if (strval[0] == '\0' || *endptr != '\0')
        return 0;
    if (errno == ERANGE)
        return 0;

    *val = tmpdo;
    return 1;
}

/**
 * \brief Retrieve a configuration value as a float
 *
 * \param name Name of configuration parameter to get.
 * \param val Pointer to an float that will be set the
 * configuration value.
 *
 * \retval 1 will be returned if the name is found and was properly
 * converted to a double, otherwise 0 will be returned.
 */
int ConfGetFloat(const char *name, float *val)
{
    const char *strval = NULL;
    double tmpfl;
    char *endptr;

    if (ConfGet(name, &strval) == 0)
        return 0;

    errno = 0;
    tmpfl = strtof(strval, &endptr);
    if (strval[0] == '\0' || *endptr != '\0')
        return 0;
    if (errno == ERANGE)
        return 0;

    *val = tmpfl;
    return 1;
}

/**
 * \brief Remove (and free) the provided configuration node.
 */
void ConfNodeRemove(ConfNode *node)
{
    if (node->parent != NULL)
        TAILQ_REMOVE(&node->parent->head, node, next);
    ConfNodeFree(node);
}

/**
 * \brief Remove a configuration parameter from the configuration db.
 *
 * \param name The name of the configuration parameter to remove.
 *
 * \retval Returns 1 if the parameter was removed, otherwise 0 is returned
 *   most likely indicating the parameter was not set.
 */
int ConfRemove(const char *name)
{
    ConfNode *node;

    node = ConfGetNode(name);
    if (node == NULL)
        return 0;
    else {
        ConfNodeRemove(node);
        return 1;
    }
}

/**
 * \brief Creates a backup of the conf_hash hash_table used by the conf API.
 */
void ConfCreateContextBackup(void)
{
    root_backup = root;
    root = NULL;

    return;
}

/**
 * \brief Restores the backup of the hash_table present in backup_conf_hash
 *        back to conf_hash.
 */
void ConfRestoreContextBackup(void)
{
    root = root_backup;
    root_backup = NULL;

    return;
}

/**
 * \brief De-initializes the configuration system.
 */
void ConfDeInit(void)
{
    if (root != NULL) {
        ConfNodeFree(root);
        root = NULL;
    }

    fprintf (stdout, "configuration module de-initialized");
}

static char *ConfPrintNameArray(char **name_arr, int level)
{
    static char name[128*128];
    int i;

    name[0] = '\0';
    for (i = 0; i <= level; i++) {
        strlcat(name, name_arr[i], sizeof(name));
        if (i < level)
            strlcat(name, ".", sizeof(name));
    }

    return name;
}

/**
 * \brief Dump a configuration node and all its children.
 */
void ConfNodeDump(const ConfNode *node, const char *prefix)
{
    ConfNode *child;

    static char *name[128];
    static int level = -1;

    level++;
    TAILQ_FOREACH(child, &node->head, next) {
        name[level] = strdup(child->name);
        if (unlikely(name[level] == NULL)) {
            continue;
        }
        if (prefix == NULL) {
            fprintf (stdout, "%s = %s\n", ConfPrintNameArray(name, level),
                child->val);
        }
        else {
            fprintf (stdout, "%s.%s = %s\n", prefix,
                ConfPrintNameArray(name, level), child->val);
        }
        ConfNodeDump(child, prefix);
        free(name[level]);
    }
    level--;
}

/**
 * \brief Dump configuration to stdout.
 */
void ConfDump(void)
{
    ConfNodeDump(root, NULL);
}

/**
 * \brief Lookup a child configuration node by name.
 *
 * Given a ConfNode this function will lookup an immediate child
 * ConfNode by name and return the child ConfNode.
 *
 * \param node The parent configuration node.
 * \param name The name of the child node to lookup.
 *
 * \retval A pointer the child ConfNode if found otherwise NULL.
 */
ConfNode *ConfNodeLookupChild(const ConfNode *node, const char *name)
{
    ConfNode *child;

    if (node == NULL || name == NULL) {
        return NULL;
    }

    TAILQ_FOREACH(child, &node->head, next) {
        if (child->name != NULL && strcmp(child->name, name) == 0)
            return child;
    }

    return NULL;
}

/**
 * \brief Lookup the value of a child configuration node by name.
 *
 * Given a parent ConfNode this function will return the value of a
 * child configuration node by name returning a reference to that
 * value.
 *
 * \param node The parent configuration node.
 * \param name The name of the child node to lookup.
 *
 * \retval A pointer the child ConfNodes value if found otherwise NULL.
 */
const char *ConfNodeLookupChildValue(const ConfNode *node, const char *name)
{
    ConfNode *child;

    child = ConfNodeLookupChild(node, name);
    if (child != NULL)
        return child->val;

    return NULL;
}

/**
 * \brief Lookup for a key value under a specific node
 *
 * \return the ConfNode matching or NULL
 */

ConfNode *ConfNodeLookupKeyValue(const ConfNode *base, const char *key,
    const char *value)
{
    ConfNode *child;

    TAILQ_FOREACH(child, &base->head, next) {
        if (!strncmp(child->val, key, strlen(child->val))) {
            ConfNode *subchild;
            TAILQ_FOREACH(subchild, &child->head, next) {
                if ((!strcmp(subchild->name, key)) && (!strcmp(subchild->val, value))) {
                    return child;
                }
            }
        }
    }

    return NULL;
}

/**
 * \brief Test if a configuration node has a true value.
 *
 * \param node The parent configuration node.
 * \param name The name of the child node to test.
 *
 * \retval 1 if the child node has a true value, otherwise 0 is
 *     returned, even if the child node does not exist.
 */
int ConfNodeChildValueIsTrue(const ConfNode *node, const char *key)
{
    const char *val;

    val = ConfNodeLookupChildValue(node, key);

    return val != NULL ? ConfValIsTrue(val) : 0;
}

/**
 *  \brief Create the path for an include entry
 *  \param file The name of the file
 *  \retval str Pointer to the string path + sig_file
 */
char *ConfLoadCompleteIncludePath(const char *file)
{
    const char *defaultpath = NULL;
    char *path = NULL;

    /* Path not specified */
    if (path_is_relative(file)) {
        if (ConfGet("include-path", &defaultpath) == 1) {
            fprintf (stdout, "Default path: %s", defaultpath);
            size_t path_len = sizeof(char) * (strlen(defaultpath) +
                          strlen(file) + 2);
            path = malloc(path_len);
            if (unlikely(path == NULL))
                return NULL;
            strlcpy(path, defaultpath, path_len);
            if (path[strlen(path) - 1] != '/')
                strlcat(path, "/", path_len);
            strlcat(path, file, path_len);
       } else {
            path = strdup(file);
            if (unlikely(path == NULL))
                return NULL;
        }
    } else {
        path = strdup(file);
        if (unlikely(path == NULL))
            return NULL;
    }
    return path;
}

/**
 * \brief Prune a configuration node.
 *
 * Pruning a configuration is similar to freeing, but only fields that
 * may be overridden are, leaving final type parameters.  Additional
 * the value of the provided node is also free'd, but the node itself
 * is left.
 *
 * \param node The configuration node to prune.
 */
void ConfNodePrune(ConfNode *node)
{
    ConfNode *item, *it;

    for (item = TAILQ_FIRST(&node->head); item != NULL; item = it) {
        it = TAILQ_NEXT(item, next);
        if (!item->final) {
            ConfNodePrune(item);
            if (TAILQ_EMPTY(&item->head)) {
                TAILQ_REMOVE(&node->head, item, next);
                if (item->name != NULL)
                    free(item->name);
                if (item->val != NULL)
                    free(item->val);
                free(item);
            }
        }
    }

    if (node->val != NULL) {
        free(node->val);
        node->val = NULL;
    }
}

/**
 * \brief Check if a node is a sequence or node.
 *
 * \param node the node to check.
 *
 * \return 1 if node is a seuence, otherwise 0.
 */
int ConfNodeIsSequence(const ConfNode *node)
{
    return node->is_seq == 0 ? 0 : 1;
}

