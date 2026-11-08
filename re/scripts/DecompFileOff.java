import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import java.util.*;
// args: hex FILE offsets of data; decompiles functions referencing them (and callers one level up)
public class DecompFileOff extends GhidraScript {
  public void run() throws Exception {
    DecompInterface di=new DecompInterface(); di.openProgram(currentProgram);
    Set<Function> done=new HashSet<>();
    for (String a: getScriptArgs()) {
      long off=Long.parseLong(a.replace("0x",""),16);
      List<Address> ads=currentProgram.getMemory().locateAddressesForFileOffset(off);
      for(Address ad: ads){
        println("fileoff "+a+" -> "+ad);
        for(Reference r: getReferencesTo(ad)){
          Function f=getFunctionContaining(r.getFromAddress());
          if(f==null||!done.add(f)) continue;
          DecompileResults res=di.decompileFunction(f,60,monitor);
          println("===== "+f.getName()+" @ "+f.getEntryPoint());
          println(res.getDecompiledFunction()==null?"(fail)":res.getDecompiledFunction().getC());
          for(Reference cr: getReferencesTo(f.getEntryPoint())){
            Function c=getFunctionContaining(cr.getFromAddress());
            if(c!=null) println("   called from "+c.getName()+" @ "+c.getEntryPoint());
          }
        }
      }
    }
  }
}
