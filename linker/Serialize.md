# Serialization format for bytecode

## Requirements

- Bytecode needs to be position independant, meaning addresses get patched at runtime. Whether it will all be relative jumps or there are some absolut ones, is an implementation detail

- All used/exported types need to be stored. This detects incompatible ABI's by checking whether the data layout of any dependant structures has changed. Types with no external linkage(Currently not implemented need not be saved)

- Easy to parse. All metadata should be located in the front for when only the name and version needs to be known. Otherwise the file should lend itself to being mapped into system memory. This should be trivial.

- Ideally the metadata should be human readable

- Reusable. Ideally also use it for interface files for solving cyclic dependencies and native modules.

- Not bound to files. Ideally this format should also work when received through a pipe or similar. (Looking at you Java)

- Fast to serialize/deserialize. Compression should be avoided.

## Layout

- Magic text: VWA\_MODULE\n
- Name length\n
- Name\n
- Exports\n
- See exports
- Imports\n
- pretty similar to exports, see relevant section
- Code\n
- Length\n
- Raw code

note: to reduce duplication, modules depend only on explicitely used types. Dependant types are loaded by the module exporting them. Nevermind. Stupid idea. Won't work. Example: depend on S a, definition of a member changes. Not detected. Needs to clone entire sym table.
### Exports
Nevermind. Need all fields. Otherwise can't use as interface file.
#### Required fields

- Name
- List of members or return type and arguments
- location if applicable

### Imports

#### Required fields 

- name
- list of member types or rt and args

## Size estimate:
required to reduce copying. If not feasable split into smaller chunks and combine at end.

# Thought:
Serialization:
assume you use a simple type with no need of extra functions e.g vec2. It is only used withing a function, so now in/output. Should this be listed as a dependency? I think not. After all, changes to the abi do not affect the function of the existing code

Why am I not using a vector or sthg of base types? What if someone swaps two elements? It would still run but produce garbage