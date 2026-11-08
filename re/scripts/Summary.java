import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
public class Summary extends GhidraScript {
  public void run() throws Exception {
    FunctionManager fm=currentProgram.getFunctionManager();
    int n=0,named=0,lib=0; long bytes=0;
    for(Function f: fm.getFunctions(true)){ n++; bytes+=f.getBody().getNumAddresses();
      String s=f.getName(); if(!s.startsWith("FUN_")) named++;
      if(f.getSymbol().getSource().toString().contains("ANALYSIS")) {} }
    println("functions="+n+" named="+named+" bytes="+bytes);
  }
}
