// List callers (function names + call-site address) of given symbols (substring match on mangled or demangled name).
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;
public class ListXref extends GhidraScript {
    public void run() throws Exception {
        String[] a = getScriptArgs();
        PrintWriter out = new PrintWriter(new FileWriter(a[0]));
        FunctionManager fm = currentProgram.getFunctionManager();
        for (int i = 1; i < a.length; i++) {
            out.println("## " + a[i]);
            for (Function f : fm.getFunctions(true)) {
                if (!f.getName(true).equals(a[i]) && !f.getName().equals(a[i])) continue;
                out.println("# target " + f.getName(true) + " @ " + f.getEntryPoint());
                TreeMap<String,List<String>> m = new TreeMap<>();
                for (Reference r : getReferencesTo(f.getEntryPoint())) {
                    Function c = fm.getFunctionContaining(r.getFromAddress());
                    String n = c == null ? "?" : c.getName(true) + " @ " + c.getEntryPoint();
                    m.computeIfAbsent(n, k -> new ArrayList<>()).add(r.getFromAddress().toString());
                }
                for (Map.Entry<String,List<String>> e : m.entrySet()) out.println(e.getKey() + "  x" + e.getValue().size());
            }
        }
        out.close();
    }
}
