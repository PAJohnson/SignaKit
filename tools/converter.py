import argparse
import yaml
import re
import sys

# Type sizes in bytes
TYPE_SIZES = {
    'char': 1,
    'int8_t': 1, 'uint8_t': 1,
    'short': 2, 'unsigned short': 2, 'int16_t': 2, 'uint16_t': 2,
    'int': 4, 'unsigned int': 4, 'int32_t': 4, 'uint32_t': 4, 'float': 4,
    'long': 8, 'unsigned long': 8, 'int64_t': 8, 'uint64_t': 8, 'double': 8,
}

class Field:
    def __init__(self, name, type_name, is_array=False, array_size=0, offset=0):
        self.name = name
        self.type_name = type_name
        self.is_array = is_array
        self.array_size = array_size
        self.offset = offset

    @property
    def size(self):
        base_size = TYPE_SIZES.get(self.type_name, 1) # Default to 1 if unknown
        return base_size * (self.array_size if self.is_array else 1)

class StructDef:
    def __init__(self, name):
        self.name = name
        self.fields = []
        self.header_string = ""
        self.size = 0

def parse_header_file(filename):
    with open(filename, 'r') as f:
        lines = f.readlines()

    structs = []
    
    in_struct = False
    current_struct_name = ""
    brace_count = 0
    buffer = ""

    # Simple tokenizer/parser loop
    # We will process line by line to easier handle start declarations
    
    # Regex to detect start of struct
    struct_start_pattern = re.compile(r'struct\s+__attribute__\(\(packed\)\)\s+(\w+)')

    for line in lines:
        stripped = line.strip()
        # Remove single line comments (simple approximation)
        if '//' in stripped:
            stripped = stripped.split('//')[0].strip()
        
        if not stripped: continue

        if not in_struct:
            match = struct_start_pattern.search(line)
            if match:
                current_struct_name = match.group(1)
                in_struct = True
                brace_count = 0
                buffer = ""
                # If the line contains '{', process it
                if '{' in line:
                    brace_count += line.count('{')
                    brace_count -= line.count('}')
                    # Extract starting from brace?
                    # Simplify: just buffer everything from this line onwards?
                    # The declaration line might be `struct ... name {`
                    # We want the body.
                    # Let's accumulate everything into a buffer filtering the struct decl part if possible
                    # Or just parse the buffer later.
        
        if in_struct:
            # Update brace count
            # Note: Need to be careful not to double count if we processed start line above
            # Let's just accumulate raw lines and count braces on the fly
            
            # If we just started, check if braces are there
            if buffer == "" and current_struct_name in line:
                 # It's the declaration line.
                 # Only count braces here
                 brace_count += line.count('{')
                 brace_count -= line.count('}')
            else:
                 brace_count += line.count('{')
                 brace_count -= line.count('}')

            buffer += line
            
            # Check if finished
            if brace_count == 0 and ('};' in line or buffer.strip().endswith('};')):
                in_struct = False
                
                # Now parse the buffer
                # Buffer contains: struct ... { body ... };
                # We want body.
                start = buffer.find('{')
                end = buffer.rfind('}')
                
                if start != -1 and end != -1:
                    body = buffer[start+1:end]
                    
                    s = StructDef(current_struct_name)
                    statements = [stmt.strip() for stmt in body.split(';') if stmt.strip()]
                    current_offset = 0
                    
                    for stmt in statements:
                         # Skip empty or whitespace
                         if not stmt: continue
                         
                         # Check for header init: char header[4] = { ... }
                         # The split(';') might have split the array init if it had semicolons (shouldn't happen in C init)
                         # But wait. `char header[4] = { ... };` -> split(';') -> `char header... { ... }` (good)
                         
                         stmt = stmt.replace('\n', ' ')
                         
                         if 'header[' in stmt and '=' in stmt:
                            chars = re.findall(r"'([^'])'", stmt)
                            s.header_string = "".join(chars).replace('\0', '')
                         
                         decl_part = stmt.split('=')[0].strip()
                         parts = decl_part.split()
                         if len(parts) < 2: continue
                         
                         field_name_full = parts[-1]
                         field_type = " ".join(parts[:-1])
                         
                         is_array = False
                         array_size = 0
                         field_name = field_name_full
                         
                         if '[' in field_name_full:
                            is_array = True
                            name_match = re.search(r'(\w+)\[(\d+)\]', field_name_full) # Use search not match
                            if name_match:
                                field_name = name_match.group(1)
                                array_size = int(name_match.group(2))
                         
                         field = Field(field_name, field_type, is_array, array_size, current_offset)
                         
                         # Handle header specially again? 
                         # If we keep header in fields, generate_yaml ignores it.
                         
                         s.fields.append(field)
                         current_offset += field.size
                    
                    s.size = current_offset
                    structs.append(s)

    return structs

def generate_yaml(structs, filename):
    data = {"packets": []}
    
    for s in structs:
        # Heuristic: Packet ID = Struct Name with "Data" stripped?
        # User said "IMUData" -> "IMU"
        pkt_id = s.name
        if pkt_id.endswith("Data"):
            pkt_id = pkt_id[:-4]
            
        packet = {
            "id": pkt_id,
            "header_string": s.header_string if s.header_string else pkt_id,
            "size_check": s.size,
            "time_field": "time", # Default assumption
            "fields": []
        }
        
        for f in s.fields:
            # Skip header field in YAML fields definition? 
            # The YAML I made earlier did NOT include 'header', it started at offset 4.
            # Let's keep consistency.
            if f.name == "header": continue
            
            field_def = {
                "name": f.name,
                "type": f.type_name,
                "offset": f.offset
            }
            packet["fields"].append(field_def)
            
        data["packets"].append(packet)
        
    with open(filename, 'w') as f:
        yaml.dump(data, f, sort_keys=False)
        
def parse_yaml_file(filename):
    with open(filename, 'r') as f:
        data = yaml.safe_load(f)
        
    structs = []
    for pkt in data.get("packets", []):
        s = StructDef(pkt["id"] + "Data")
        s.header_string = pkt.get("header_string", "")
        # Reconstruct fields and sizes?
        # Actually we just need to write the file.
        # But to write the struct, we need fields.
        
        # We need to assume types for standard fields like Header if they are missing from YAML fields
        # YAML fields usually start after header.
        
        # Add Header
        if s.header_string:
            # We need to recreate the header field
            # char header[4] = ...
            header_chars = list(s.header_string)
            if len(header_chars) < 4:
                header_chars.extend(['\\0'] * (4 - len(header_chars)))
            # This logic is for writing, let's just enable writing
            pass
            
        s.fields = []
        for f in pkt.get("fields", []):
             # We assume offset is correct or we can recalculate it? 
             # For C generation, we just print fields in order.
             # We should probably sort by offset to be safe.
             field = Field(f["name"], f["type"], offset=f["offset"])
             s.fields.append(field)
             
        # Sort fields by offset
        s.fields.sort(key=lambda x: x.offset)
        structs.append(s)
        
    return structs

def generate_header(structs, filename):
    with open(filename, 'w') as f:
        f.write("#pragma once\n")
        f.write("#include <cstdint>\n\n")
        
        for s in structs:
            f.write(f"struct __attribute__((packed)) {s.name}\n")
            f.write("{\n")
            
            # Check if we need to insert header manually
            # If the first field starts at 4, we assume 0-4 is header?
            first_offset = s.fields[0].offset if s.fields else 0
            if first_offset >= 4 and s.header_string:
                # Write header
                chars = ", ".join([f"'{c}'" for c in s.header_string] + ["'\\0'"] * (4 - len(s.header_string)))
                f.write(f"    char header[4] = {{{chars}}};\n")
            
            for field in s.fields:
                f.write(f"    {field.type_name} {field.name};\n")
                
            f.write("};\n\n")

def main():
    parser = argparse.ArgumentParser(description="Convert between C Header and YAML signals config")
    parser.add_argument("--to-yaml", nargs=2, metavar=('INPUT_HEADER', 'OUTPUT_YAML'), help="Convert Header to YAML")
    parser.add_argument("--to-header", nargs=2, metavar=('INPUT_YAML', 'OUTPUT_HEADER'), help="Convert YAML to Header")
    
    args = parser.parse_args()
    
    if args.to_yaml:
        structs = parse_header_file(args.to_yaml[0])
        generate_yaml(structs, args.to_yaml[1])
        print(f"Converted {args.to_yaml[0]} to {args.to_yaml[1]}")
    elif args.to_header:
        structs = parse_yaml_file(args.to_header[0])
        generate_header(structs, args.to_header[1])
        print(f"Converted {args.to_header[0]} to {args.to_header[1]}")
    else:
        parser.print_help()

if __name__ == "__main__":
    main()
