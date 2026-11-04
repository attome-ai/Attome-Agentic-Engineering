#include <IFSelect_ReturnStatus.hxx>
#include <Message.hxx>
#include <Message_Messenger.hxx>
#include <Message_PrinterOStream.hxx>
#include <Interface_Static.hxx>
#include <STEPControl_Reader.hxx>
#include <STEPControl_Writer.hxx>

#include "structura/design.hpp"

namespace structura {

// OCCT prints transfer statistics to stdout; the engine's stdout carries JSON only, so remove its printers once.
static void quiet_occt() {
  static bool done = false;
  if (done) return;
  done = true;
  Message::DefaultMessenger()->RemovePrinters(STANDARD_TYPE(Message_PrinterOStream));
}

void write_step(const TopoDS_Shape& s, const std::string& path) {
  quiet_occt();
  STEPControl_Writer w;
  Interface_Static::SetCVal("write.step.unit", "MM");
  if (w.Transfer(s, STEPControl_AsIs) != IFSelect_RetDone) throw std::runtime_error("STEP transfer failed");
  if (w.Write(path.c_str()) != IFSelect_RetDone) throw std::runtime_error("cannot write STEP to " + path);
}

TopoDS_Shape read_step(const std::string& path) {
  quiet_occt();
  STEPControl_Reader r;
  if (r.ReadFile(path.c_str()) != IFSelect_RetDone) throw std::runtime_error("cannot read STEP " + path);
  r.TransferRoots();
  return r.OneShape();
}

}  // namespace structura
