import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import java.io.*;
// args: OUTFILE START END [START END ...] (hex) - decompiles every function with an entry in a range
public class DecompRange extends GhidraScript {
  public void run() throws Exception {
    String[] a = getScriptArgs();
    DecompInterface di = new DecompInterface(); di.openProgram(currentProgram);
    PrintWriter out = new PrintWriter(new FileWriter(a[0]));
    int n = 0;
    for (int i = 1; i + 1 < a.length; i += 2) {
    Address s = toAddr(a[i]), e = toAddr(a[i + 1]);
    for (Function f : currentProgram.getFunctionManager().getFunctions(s, true)) {
      if (f.getEntryPoint().compareTo(e) >= 0) break;
      DecompileResults r = di.decompileFunction(f, 60, monitor);
      out.println("===== " + f.getName() + " @ " + f.getEntryPoint());
      out.println(r.getDecompiledFunction() == null ? "(fail)" : r.getDecompiledFunction().getC());
      n++;
    }
    }
    out.close();
    println("decompiled " + n);
  }
}
