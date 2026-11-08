import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import java.io.*;
import java.nio.file.*;
// args: INFILE OUTFILE. INFILE lines "ADDR NAME" (hex). Disassembles and creates a
// function where none exists (Delphi published methods are only reachable via
// method tables), names it, and decompiles it.
public class DecompList extends GhidraScript {
  public void run() throws Exception {
    String[] a = getScriptArgs();
    java.util.List<String> lines = Files.readAllLines(Paths.get(a[0]));
    for (String l : lines) {
      String[] p = l.trim().split("\\s+");
      Address ad = toAddr(p[0]);
      if (getFunctionAt(ad) == null) {
        disassemble(ad);
        Function f = createFunction(ad, null);
        if (f != null && p.length > 1) f.setName(p[1], ghidra.program.model.symbol.SourceType.USER_DEFINED);
      } else if (p.length > 1) {
        getFunctionAt(ad).setName(p[1], ghidra.program.model.symbol.SourceType.USER_DEFINED);
      }
    }
    DecompInterface di = new DecompInterface(); di.openProgram(currentProgram);
    PrintWriter out = new PrintWriter(new FileWriter(a[1]));
    for (String l : lines) {
      String[] p = l.trim().split("\\s+");
      Function f = getFunctionAt(toAddr(p[0]));
      out.println("===== " + (p.length > 1 ? p[1] : "?") + " @ " + p[0]);
      if (f == null) { out.println("(no function)"); continue; }
      DecompileResults r = di.decompileFunction(f, 60, monitor);
      out.println(r.getDecompiledFunction() == null ? "(fail)" : r.getDecompiledFunction().getC());
    }
    out.close();
  }
}
