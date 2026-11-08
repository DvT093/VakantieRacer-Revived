import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.address.*;
import java.util.*;
// args: hex values; lists instructions with that scalar/address operand, decompiles containing functions
public class FindScalar extends GhidraScript {
  public void run() throws Exception {
    DecompInterface di=new DecompInterface(); di.openProgram(currentProgram);
    Set<Function> done=new HashSet<>();
    long[] vals=Arrays.stream(getScriptArgs()).mapToLong(s->Long.parseLong(s.replace("0x",""),16)).toArray();
    InstructionIterator it=currentProgram.getListing().getInstructions(true);
    while(it.hasNext()){
      Instruction ins=it.next();
      for(int i=0;i<ins.getNumOperands();i++) for(Object o: ins.getOpObjects(i)){
        long v=-1;
        if(o instanceof Scalar) v=((Scalar)o).getUnsignedValue();
        else if(o instanceof Address) v=((Address)o).getOffset();
        for(long t: vals) if(v==t){
          Function f=getFunctionContaining(ins.getAddress());
          println("HIT "+Long.toHexString(t)+" at "+ins.getAddress()+" "+ins+" in "+(f==null?"?":f.getName()+"@"+f.getEntryPoint()));
          if(f!=null&&done.add(f)){
            DecompileResults r=di.decompileFunction(f,60,monitor);
            println(r.getDecompiledFunction()==null?"(fail)":r.getDecompiledFunction().getC());
          }
        }
      }
    }
  }
}
