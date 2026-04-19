# PARTIO

### ***PARTIO*** is a component of the "***NOX***" project, a fundamental rethinking and research initiative aimed at optimizing configuration and build systems. It serves as a specialized interpreter for the ***YACS*** (Yet Another Configuration System) DSL.
---
### Warning! This project is under active development. Bugs and broken code are possible before the release.
---

**Key Advantages**
* **Guaranteed Determinism**: The system describes a precise, immutable task graph where the configuration block remains the single source of truth.

* **No Hidden Side Effects**: There is no implicit influence from environment variables or the specific order of function calls.

* **Early Error Detection**: Thanks to a strict grammar and a robust typing system — utilizing a _Value_ type integrated over _std::variant_ — syntax errors and type mismatches are caught during the initial syntax analysis stage.

* **Zero State Leakage**: Each target and project is syntactically isolated, ensuring that variables do not propagate implicitly down the dependency tree.

* **Explicit Data Access**: A target only has access to information that has been explicitly passed to it.

**Extensibility and Modularity**

***PARTIO*** addresses a major "pain point" of legacy systems: the creation of monolithic configuration files that span thousands of lines. ***YACS*** is designed from the ground up with composition in mind:

* **External File Support**: Complex logic, macros, or project-specific descriptions can be offloaded into separate YACS scripts.

* **Logic Reusability**: This modular approach enables the creation of shared libraries for build scenarios (e.g., standard compiler flags or deployment rules) that are imported only where they are genuinely needed.

**Technical Architecture**

The interpreter is engineered for high-performance lexical analysis and strict type safety within the C++ ecosystem:

* **Modern Engine:** The core is written in C++26, making active use of std::ranges, std::move_only_function, and C++ modules.

* **Formal Grammar:**
	    The parser is generated via ANTLR4 based on a strict formal grammar, avoiding the pitfalls of unreliable regex-based parsing.

* **Type-Safe C++ Integration:**
	    Built-in language functions are registered through C++ wrappers that feature automatic return type deduction.

**Syntax example**:
```cpp
//This is a single-line comment.

/*
* This is a
* multi-line
* comment.
*/

//Import "somelib.yacs" and assign it the alias "lib", accessed via "lib:somesunc"
import somelib as lib;

project [[
	// These are all global variables,
	// accessible by build targets.
	name = "My great project";
	version = "1.0.0";
	my_var = "blablabla";

	// Isolated target block
	target build_core [[
		//Built-in variables in the target
		type = executable;
		depends = [ example2, example3, antlr:runtime ];
		sources = $source_files;
		sources += $addition_files;
		includes = ["/dasda/dsada/gfre", "/dasda/dsada/jye"];

		//Local variable of the specified type, pre-computed
		bool a = lib:foo:bar(1, 2);

		if(a) {
			flags = ["-flag3", "-flag4"];
		} else {
			flags = ["-flag1", "-flag2"];
		}
	]]
]]
```
