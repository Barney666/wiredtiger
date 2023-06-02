#!/usr/bin/env python

from __future__ import print_function
import os, re, subprocess, sys, textwrap
from dist import compare_srcfile, format_srcfile, ModifyFile

test_config = False

# This file serves two purposes, it can generate configuration for the main wiredtiger library and,
# it can generate configuration for the c and cpp suite tests. To avoid duplication we import the
# differing apis here and then treat them as the same for the remainder of the script. However we
# do have different logic depending on whether we intend to generate the test api or not, which is
# managed with a boolean flag.
if len(sys.argv) == 1 or sys.argv[1] != "-t":
    import api_data as api_data_def
else:
    test_config = True
    import test_data as api_data_def

tmp_file = '__tmp'

#####################################################################
# Update wiredtiger.in with doxygen comments
#####################################################################
f='../src/include/wiredtiger.in'
tfile = open(tmp_file, 'w')

whitespace_re = re.compile(r'\s+')
cbegin_re = re.compile(r'(\s*\*\s*)@config(?:empty|start)\{(.*?),.*\}')

def gen_id_name(name, ty):
    id_name = name
    if ty == 'category':
        # Generate a different name for the category.  Our API has several instances
        # where we have a category name, like 'checkpoint' in one API call, that is also a key
        # name in a different API call.  We don't care that they appear multiple times,
        # but we must be able to distinguish between IDs that we'll generate for category
        # names vs regular key names.
        id_name = id_name[0].upper() + id_name[1:]
    return id_name

# Remember a set of key names, assigning a unique number to each one.
class KeyNumber:
    def __init__(self):
        self.numbering = dict()
        self.count = 0

    def add(self, name):
        if not name in self.numbering:
            num = self.count
            self.count += 1
            self.numbering[name] = num

    def get(self, name):
        return self.numbering[name]

def gettype(c):
    '''Derive the type of a config item'''
    checks = c.flags
    ctype = checks.get('type', None)
    if not ctype and ('min' in checks or 'max' in checks):
        ctype = 'int'
    return ctype or 'string'

def typedesc(c):
    '''Descripe what type of value is expected for the given config item'''
    checks = c.flags
    cmin = str(checks.get('min', ''))
    cmax = str(checks.get('max', ''))
    choices = checks.get('choices', [])
    ctype = gettype(c)
    desc = {
        'boolean' : 'a boolean flag',
        'format'  : 'a format string',
        'int'     : 'an integer',
        'list'    : 'a list',
        'category': 'a set of related configuration options defined as follows',
        'string'  : 'a string'}[ctype]
    if cmin and cmax:
        desc += ' between \c ' + cmin + ' and \c ' + cmax
    elif cmin:
        desc += ' greater than or equal to \c ' + cmin
    elif cmax:
        desc += ' no more than \c ' + cmax
    if choices:
        if ctype == 'list':
            desc += ', with values chosen from the following options: '
        else:
            desc += ', chosen from the following options: '
        desc += ', '.join('\\c "' + c + '"' for c in choices)
    elif ctype == 'list':
        desc += ' of strings'
    return desc

def parseconfig(c, method_name, name_indent=''):
    c.method_name = method_name
    ctype = gettype(c)
    desc = whitespace_re.sub(' ', c.desc.strip())
    desc = desc.strip('.') + '.'
    desc = desc.replace(',', '\\,')
    default = '\\c ' + str(c.default) if c.default or ctype == 'int' \
            else 'empty'
    name = name_indent + c.name

    tdesc = typedesc(c)
    if ctype != 'category':
        tdesc += '; default ' + default
    else:
        name += ' = ('
    tdesc += '.'
    tdesc = tdesc.replace(',', '\\,')
    output = '@config{' + ', '.join((name, desc, tdesc)) + '}\n'
    if ctype == 'category':
        for subc in sorted(c.subconfig):
            if 'undoc' in subc.flags:
                continue
            output += parseconfig(subc, method_name, \
                                name_indent + ('&nbsp;' * 4))
        output += '@config{' + name_indent + ' ),,}\n'
    return output

# Convert a string to a number string that can be parsed by the C compiler.
# All numbers are decimal, but with the possibility of 'K', 'Mb', 'GB', etc. appended
def getcompnum(s):
    # Already converted?
    if type(s) == int:
        return str(s)
    result = re.search(r'(\d+)([bBkKmMgGtTpP]*)', s)
    num = int(result.group(1))
    mult = ''
    for ch in result.group(2):
        ch = ch.lower()
        if ch == 'b':
            pass    # No change.  Useful for example '10GB'
        elif ch == 'k':
            mult += ' * WT_KILOBYTE'
        elif ch == 'm':
            mult += ' * WT_MEGABYTE'
        elif ch == 'g':
            mult += ' * WT_GIGABYTE'
        elif ch == 't':
            mult += ' * WT_TERABYTE'
        elif ch == 'p':
            mult += ' * WT_PETABYTE'

    # If numbers are large or have a multiplier, make it a long long literal
    # so it won't overflow.
    if num > 1000000 or len(mult) > 0:
        num = str(num) + 'LL'
    else:
        num = str(num)
    return num + mult

choices_names = set()
# Get fields that assist the configuration compiler.
def getcompstr(c, keynumber):
    comptype = -1
    ty = gettype(c)
    # E.g. "WT_CONFIG_COMPILED_TYPE_INT"
    comptype = 'WT_CONFIG_COMPILED_TYPE_' + ty.upper()
    offset = keynumber.get(gen_id_name(c.name, ty))
    checks = c.flags
    minval = 'INT64_MIN'
    maxval = 'INT64_MAX'
    if 'min' in checks:
        minval = getcompnum(checks['min'])
    if 'max' in checks:
        maxval = getcompnum(checks['max'])
    choices = checks.get('choices', [])
    if len(choices) == 0:
        choices_ref = 'NULL'
    else:
        name = c.name
        suffix = 1
        while name in choices_names:
            suffix += 1
            name = c.name + str(suffix)
        choices_names.add(name)
        choices_ref = 'confchk_' + name + '_choices'
        tfile.write('''
        %(name)s[] = {
        \t%(values)s
        \tNULL
        };
        ''' % {
            'name' : '\n'.join(ws.wrap('static const char *' + choices_ref)),
            'values' : '\n\t'.join('"' + choice + '",' for choice in choices),
        })

    return ', {}, {}, {}, {}, {}'.format(comptype, offset, minval, maxval, choices_ref)

def getconfcheck(c, keynumber):
    check = '{ "' + c.name + '", "' + gettype(c) + '",'
    cstr = checkstr(c)
    sstr = getsubconfigstr(c, keynumber) + getcompstr(c, keynumber)
    if cstr != 'NULL':
        cstr = '"\n\t    "'.join(w.wrap(cstr))
        # Manually re-wrap when there is a check string to avoid ugliness
        # between string and non-string wrapping
        if len(check + ' ' + cstr + ',\n\t    ' + sstr + '},') >= 68:
            check = check + '\n\t    ' + cstr + ',\n\t    ' + sstr + ' },'
        else:
            check = check + ' ' + cstr + ', ' + sstr + ' },'
    else:
        check = '\n\t    '.join(
            w.wrap(check + ' ' + cstr + ', ' + sstr + ' },'))
    return check

def add_conf_keys_one(c, conf_keys):
    ctype = gettype(c)
    idname = gen_id_name(c.name, ctype)
    if ctype == 'category':
        subconf_keys = dict()
        add_conf_keys(c.subconfig, subconf_keys, False)
        conf_keys[idname] = subconf_keys
    else:
        if idname in conf_keys:
            curval = conf_keys[idname]
            assert type(curval) == int, 'type conflict for name={}'.format(c.name)
        else:
            curval = 0
        curval += 1
        conf_keys[idname] = curval

def add_conf_keys(container, conf_keys, is_top):
    if is_top:
        l = sorted(container.keys())
    else:
        l = sorted(container)
    for element in l:
        if is_top:
            config = container[element].config
            if config:
                for c in config:
                    add_conf_keys_one(c, conf_keys)
        else:
            config = element
            add_conf_keys_one(config, conf_keys)

config_names = dict()
config_long_names = dict()
config_key = []      # sorted by name

keynumber = KeyNumber()

# Before we start any output, walk through all configuration keys, including
# subcategories, and register the names. Each unique name will get a unique number.
def add_keys(keynumber, configs, prefix):
    global config_num, config_key
    for c in configs:
        ty = gettype(c)
        idname = gen_id_name(c.name, ty)
        keynumber.add(idname)
        if not c.name in config_key:
            config_names[c.name] = c.name
        config_long_names[prefix + c.name] = -1
        if ty == 'category':
            add_keys(keynumber, c.subconfig, prefix + c.name + '.')

for name in api_data_def.methods.keys():
    method = api_data_def.methods[name]
    config = method.config
    if config:
        add_keys(keynumber, config, '')

if not test_config:
    for name in sorted(config_names):
        config_key.append(name)

    skip = False
    for line in open(f, 'r'):
        if skip:
            if '@configend' in line:
                skip = False
            continue

        m = cbegin_re.match(line)
        if not m:
            tfile.write(line)
            continue

        prefix, config_name = m.groups()
        if config_name not in api_data_def.methods:
            print("Missing configuration for " + config_name, file=sys.stderr)
            tfile.write(line)
            continue

        skip = ('@configstart' in line)

        if not api_data_def.methods[config_name].config:
            tfile.write(prefix + '@configempty{' + config_name +
                    ', see dist/api_data.py}\n')
            continue

        tfile.write(prefix + '@configstart{' + config_name +
                ', see dist/api_data.py}\n')

        w = textwrap.TextWrapper(width=100-len(prefix.expandtabs()),
                break_on_hyphens=False,
                break_long_words=False,
                replace_whitespace=False,
                fix_sentence_endings=True)
        # Separate at spaces, and after a set of non-breaking space indicators.
        w.wordsep_re = w.wordsep_simple_re = \
            re.compile(r'(\s+|(?<=&nbsp;)[\w_,.;:]+)')
        for c in api_data_def.methods[config_name].config:
            if 'undoc' in c.flags:
                continue
            output = parseconfig(c, config_name)
            for l in w.wrap(output):
                tfile.write(prefix + l.replace('\n', '\n' + prefix) + '\n')

        tfile.write(prefix + '@configend\n')

    tfile.close()
    compare_srcfile(tmp_file, f)
    # Don't add wiredtiger.in to the clang_format list.

#####################################################################
# Create config_def.c with defaults for each config string
#####################################################################
f='../src/config/config_def.c'
if test_config:
    f = '../src/config/test_config.c'
tfile = open(tmp_file, 'w')

tfile.write('''/* DO NOT EDIT: automatically built by dist/api_config.py. */

#include "wt_internal.h"
''')

# Make a TextWrapper that wraps at commas.
w = textwrap.TextWrapper(width=64, break_on_hyphens=False,
                         break_long_words=False)
w.wordsep_re = w.wordsep_simple_re = re.compile(r'(,)')

# TextWrapper that wraps at whitespace.
ws = textwrap.TextWrapper(width=64, break_on_hyphens=False,
                          break_long_words=False)

def checkstr(c):
    '''Generate the function reference and JSON string used by __wt_config_check
       to validate the config string'''
    checks = c.flags
    cfunc = str(checks.get('func', ''))
    if not cfunc:
        cfunc = 'NULL';
    cmin = str(checks.get('min', ''))
    cmax = str(checks.get('max', ''))
    choices = checks.get('choices', [])
    result = []
    if cmin:
        result.append('min=' + cmin)
    if cmax:
        result.append('max=' + cmax)
    if choices:
        result.append('choices=' + '[' +
            ','.join('\\"' + s + '\\"' for s in choices) + ']')
    if result:
        return cfunc + ', "' + ','.join(result) + '"'
    else:
        return cfunc + ', NULL'

def get_default(c):
    t = gettype(c)
    if c.default == 'false':
        return 'false'
    elif c.default == 'true':
        return 'true'
    elif t == 'string' and c.default == 'none' and \
        not c.flags.get('choices', []):
        return ''
    elif t == 'category':
        return '(%s)' % (','.join('%s=%s' % (subc.name, get_default(subc))
            for subc in sorted(c.subconfig)))
    elif c.default or t == 'int':
        return str(c.default).replace('"', '\\"')
    else:
        return ''

created_subconfigs=set()
def add_subconfig(c, cname, keynumber):
    if cname in created_subconfigs:
        return
    created_subconfigs.add(cname)
    tfile.write('''
%(name)s[] = {
\t%(check)s
\t{ NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, NULL }
};
''' % {
    'name' : '\n    '.join(ws.wrap(\
        'static const WT_CONFIG_CHECK confchk_' + cname + '_subconfigs')),
    'check' : '\n\t'.join(getconfcheck(subc, keynumber) for subc in sorted(c.subconfig)),
})

def getcname(c):
    '''Return the C name of a sub configuration'''
    prefix = c.method_name.replace('.', '_') + '_' \
             if hasattr(c, 'method_name') else ''
    return prefix + c.name

def getsubconfigstr(c, keynumber):
    '''Return a string indicating if an item has sub configuration'''
    ctype = gettype(c)
    if ctype == 'category':
        cname = getcname(c)
        add_subconfig(c, cname, keynumber)
        return 'confchk_' + cname + '_subconfigs, ' + str(len(c.subconfig))
    else:
        return 'NULL, 0'

# Write structures of arrays of allowable configuration options, including a
# NULL as a terminator for iteration.
for name in sorted(api_data_def.methods.keys()):
    config = api_data_def.methods[name].config
    if config:
        tfile.write('''
static const WT_CONFIG_CHECK confchk_%(name)s[] = {
\t%(check)s
\t{ NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, NULL }
};
''' % {
    'name' : name.replace('.', '_'),
    'check' : '\n\t'.join(getconfcheck(c, keynumber) for c in config),
})

# Write the initialized list of configuration entry structures.
tfile.write('\n')
tfile.write('static const WT_CONFIG_ENTRY config_entries[] = {')

slot=-1
config_defines = ''
for name in sorted(api_data_def.methods.keys()):
    method = api_data_def.methods[name]
    config = method.config
    compilable = method.compilable
    slot += 1

    # Build a list of #defines that reference specific slots in the list (the
    # #defines are used to avoid a list search where we know the correct slot).
    config_defines +=\
        '#define WT_CONFIG_ENTRY_' + name.replace('.', '_') + ' ' + \
            str(slot) + '\n'

    # Write the method name and base.
    tfile.write('''
\t{ "%(name)s",
%(config)s,''' % {
    'config' : '\n'.join('\t  "%s"' % line
        for line in w.wrap(','.join('%s=%s' % (c.name, get_default(c))
            for c in config)) or [""]),
    'name' : name
})

    # Write the checks reference, or NULL if no related checks structure.
    tfile.write('\n\t  ')
    if config:
        tfile.write(
            'confchk_' + name.replace('.', '_') + ', ' + str(len(config)))
    else:
        tfile.write('NULL, 0')

    tfile.write(', ' + str(slot + 1))
    if compilable:
        tfile.write(', true')
    else:
        tfile.write(', false')

    tfile.write('\n\t},')

# Write a NULL as a terminator for iteration.
tfile.write('\n\t{ NULL, NULL, NULL, 0, 0, false }')
tfile.write('\n};\n')

# Write the routine that connects the WT_CONNECTION_IMPL structure to the list
# of configuration entry structures.
if not test_config:
    tfile.write('''
    int
    __wt_conn_config_init(WT_SESSION_IMPL *session)
    {
    \tWT_CONNECTION_IMPL *conn;
    \tconst WT_CONFIG_ENTRY *ep, **epp;

    \tconn = S2C(session);

    \t/* Build a list of pointers to the configuration information. */
    \tWT_RET(__wt_calloc_def(session, WT_ELEMENTS(config_entries), &epp));
    \tconn->config_entries = epp;

    \t/* Fill in the list to reference the default information. */
    \tfor (ep = config_entries;;) {
    \t\t*epp++ = ep++;
    \t\tif (ep->method == NULL)
    \t\t\tbreak;
    \t}
    \treturn (0);
    }

    void
    __wt_conn_config_discard(WT_SESSION_IMPL *session)
    {
    \tWT_CONNECTION_IMPL *conn;

    \tconn = S2C(session);

    \t__wt_free(session, conn->config_entries);
    }

    /*
    * __wt_conn_config_match --
    *      Return the static configuration entry for a method.
    */
    const WT_CONFIG_ENTRY *
    __wt_conn_config_match(const char *method)
    {
    \tconst WT_CONFIG_ENTRY *ep;

    \tfor (ep = config_entries; ep->method != NULL; ++ep)
    \t\tif (strcmp(method, ep->method) == 0)
    \t\t\treturn (ep);
    \treturn (NULL);
    }
    ''')
else:
    tfile.write(
    '''
    /*
        * __wt_test_config_match --
        *     Return the static configuration entry for a test.
        */
    const WT_CONFIG_ENTRY *
    __wt_test_config_match(const char *test_name)
    {
        const WT_CONFIG_ENTRY *ep;

        for (ep = config_entries; ep->method != NULL; ++ep)
            if (strcmp(test_name, ep->method) == 0)
                return (ep);
        return (NULL);
    }
    '''
    )

tfile.close()
format_srcfile(tmp_file)
compare_srcfile(tmp_file, f)

# From names = ['verbose'], produce 'WT_CONF_KEY_verbose'
# From names = ['assert', 'commit_timestamp'],
#    produce 'WT_CONF_KEY_assert_CAT | (WT_CONF_KEY_archive << 16)'
def build_key_initializer(names):
    result = ''
    shift = 0

    for name in names:
        if result != '':
            result += ' | '
        if shift == 0:
            result += 'WT_CONF_KEY_{}'.format(name)
        else:
            result += '(WT_CONF_KEY_{} << {})'.format(name, shift)
        shift += 16
    return result

def gen_conf_key_struct_init(indent, names, conf_keys):
    structs = ''
    inits = ''
    for name in sorted(conf_keys):
        subnames = names.copy()
        subnames.append(name)
        h = conf_keys[name]
        if type(h) == int:
            structs += '{}uint64_t {};\n'.format(indent, name)
            inits += '{}{},\n'.format(indent, build_key_initializer(subnames))
        else:
            structs += '{}struct {}\n'.format(indent, '{')
            inits += '{}{}\n'.format(indent, '{')
            (s2, i2) = gen_conf_key_struct_init(indent + '  ', subnames, h)
            structs += s2 + '{}{} {};\n'.format(indent, '}', name)
            inits += i2 + '{}{},\n'.format(indent, '}')
    return [structs, inits]

# Update the config.h and conf.h files with the #defines for the configuration entries.
if not test_config:
    config_h = ModifyFile('../src/include/config.h')
    conf_h = ModifyFile('../src/include/conf.h')

    with config_h.replace_fragment('configuration section') as tfile:
        tfile.write(config_defines)

    conf_keys = dict()

    add_conf_keys(api_data_def.methods, conf_keys, True)

    # Assign unique numbers to all keys used in configuration, regardless of "level".
    with conf_h.replace_fragment('API configuration keys') as tfile:
        count = 0
        a = keynumber.numbering
        b = dict()
        for name in sorted(keynumber.numbering.keys()):
            off = keynumber.get(name)
            tfile.write('#define WT_CONF_KEY_{} {}ULL\n'.format(name, off))
            count += 1
            b[name] = name
        assert count == keynumber.count
        tfile.write('\n#define WT_CONF_KEY_COUNT {}\n'.format(count))

    with conf_h.replace_fragment('Configuration key structure') as tfile:
        (structs, inits) = gen_conf_key_struct_init('    ', [], conf_keys)
        tfile.write('static const struct {\n')
        tfile.write(structs)
        tfile.write('} WT_CONF_KEY_STRUCTURE = {\n')
        tfile.write(inits)
        tfile.write('};\n')

    config_h.done()
    conf_h.done()
