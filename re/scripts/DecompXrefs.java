import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.util.*;
// args: hex addresses of data; decompiles every function that references them
public class DecompXrefs extends GhidraScript {
  public void run() throws Exception {
    DecompInterface di=new DecompInterface(); di.openProgram(currentProgram);
    Set<Function> done=new HashSet<>();
    for (String a: getScriptArgs()) {
      Address ad=toAddr(a);
      for(Reference r: getReferencesTo(ad)){
        Function f=getFunctionContaining(r.getFromAddress());
        if(f==null||!done.add(f)) continue;
        DecompileResults res=di.decompileFunction(f,60,monitor);
        println("===== "+a+" -> "+f.getName()+" @ "+f.getEntryPoint());
        println(res.getDecompiledFunction()==null?"(fail)":res.getDecompiledFunction().getC());
      }
    }
  }
}
