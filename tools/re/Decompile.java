// Decompile functions by name/address, or list callers of a symbol.
// Usage (headless postScript args):
//   Decompile.java <out_file> <target> [<target> ...]
//   target: function name (exact or demangled substring prefixed with ~), 0xADDR,
//           or "xref:NAME" to dump all functions that reference NAME (decompiled).
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class Decompile extends GhidraScript {
    DecompInterface di;
    PrintWriter out;
    Set<Function> done = new HashSet<>();

    void dump(Function f) {
        if (f == null || done.contains(f)) return;
        done.add(f);
        DecompileResults r = di.decompileFunction(f, 120, monitor);
        out.println("// ===== " + f.getName() + " @ " + f.getEntryPoint() + " =====");
        if (r != null && r.decompileCompleted()) out.println(r.getDecompiledFunction().getC());
        else out.println("// decompile failed");
    }

    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        out = new PrintWriter(new FileWriter(a[0]));
        di = new DecompInterface();
        di.openProgram(currentProgram);
        FunctionManager fm = currentProgram.getFunctionManager();
        SymbolTable st = currentProgram.getSymbolTable();
        for (int i = 1; i < a.length; i++) {
            String t = a[i];
            if (t.startsWith("0x")) {
                Address ad = toAddr(t);
                Function f = fm.getFunctionContaining(ad);
                if (f == null) f = createFunction(ad, null);
                dump(f);
            } else if (t.startsWith("xref:")) {
                String n = t.substring(5);
                for (Symbol s : st.getSymbols(n)) {
                    for (Reference ref : getReferencesTo(s.getAddress())) {
                        dump(fm.getFunctionContaining(ref.getFromAddress()));
                    }
                }
            } else if (t.startsWith("~")) {
                String n = t.substring(1);
                for (Function f : fm.getFunctions(true)) {
                    if (f.getName(true).contains(n)) dump(f);
                }
            } else {
                for (Symbol s : st.getSymbols(t)) {
                    dump(fm.getFunctionAt(s.getAddress()));
                }
            }
        }
        out.close();
    }
}
