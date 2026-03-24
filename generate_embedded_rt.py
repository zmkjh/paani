#!/usr/bin/env python3
"""Generate paani_embedded_rt.hpp from SoA runtime files"""

import os

def read_file(path):
    with open(path, 'r', encoding='utf-8') as f:
        return f.read()

# Read SoA runtime files
soa_h = read_file('paani_rt/paani_ecs_soa.h')
soa_c = read_file('paani_rt/paani_ecs_soa.c')

# Modify C file: replace the original include with our generated header name
# The original is: #include "paani_ecs_soa.h"
# We replace it with: #include "paani_ecs.h"
soa_c_fixed = soa_c.replace('#include "paani_ecs_soa.h"', '#include "paani_ecs.h"')

# Generate embedded header with separate H and C content
output = '''#ifndef PAANI_EMBEDDED_RT_HPP
#define PAANI_EMBEDDED_RT_HPP

// Auto-generated embedded paani SoA runtime
// Archetype-Based SoA ECS with cache-friendly data layout

constexpr const char* PAANI_ECS_H_CONTENT = R"SOA_HEADER(
''' + soa_h + ''')SOA_HEADER";

constexpr const char* PAANI_ECS_C_CONTENT = R"SOA_SOURCE(
''' + soa_c_fixed + ''')SOA_SOURCE";

#endif // PAANI_EMBEDDED_RT_HPP
'''

# Write output
with open('paani_embedded_rt.hpp', 'w', encoding='utf-8') as f:
    f.write(output)

print("Generated paani_embedded_rt.hpp")
print(f"Header size: {len(soa_h)} bytes")
print(f"Source size: {len(soa_c_fixed)} bytes")