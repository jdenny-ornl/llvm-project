//===- RISCVTargetDefEmitter.cpp - Generate lists of RISC-V CPUs ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This tablegen backend emits the include file needed by RISCVTargetParser.cpp
// and RISCVISAInfo.cpp to parse the RISC-V CPUs and extensions.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/DenseSet.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/RISCVISAUtils.h"
#include "llvm/TableGen/Record.h"
#include "llvm/TableGen/TableGenBackend.h"

using namespace llvm;

static StringRef getExtensionName(const Record *R) {
  StringRef Name = R->getValueAsString("Name");
  Name.consume_front("experimental-");
  return Name;
}

static void printExtensionTable(raw_ostream &OS,
                                ArrayRef<const Record *> Extensions,
                                bool Experimental) {
  OS << "static const RISCVSupportedExtension Supported";
  if (Experimental)
    OS << "Experimental";
  OS << "Extensions[] = {\n";

  for (const Record *R : Extensions) {
    if (R->getValueAsBit("Experimental") != Experimental)
      continue;

    OS.indent(4) << "{\"" << getExtensionName(R) << "\", {"
                 << R->getValueAsInt("MajorVersion") << ", "
                 << R->getValueAsInt("MinorVersion") << "}},\n";
  }

  OS << "};\n\n";
}

static void emitRISCVExtensions(const RecordKeeper &Records, raw_ostream &OS) {
  OS << "#ifdef GET_SUPPORTED_EXTENSIONS\n";
  OS << "#undef GET_SUPPORTED_EXTENSIONS\n\n";

  std::vector<const Record *> Extensions =
      Records.getAllDerivedDefinitionsIfDefined("RISCVExtension");
  llvm::sort(Extensions, [](const Record *Rec1, const Record *Rec2) {
    return getExtensionName(Rec1) < getExtensionName(Rec2);
  });

  if (!Extensions.empty()) {
    printExtensionTable(OS, Extensions, /*Experimental=*/false);
    printExtensionTable(OS, Extensions, /*Experimental=*/true);
  }

  OS << "#endif // GET_SUPPORTED_EXTENSIONS\n\n";

  OS << "#ifdef GET_IMPLIED_EXTENSIONS\n";
  OS << "#undef GET_IMPLIED_EXTENSIONS\n\n";

  if (!Extensions.empty()) {
    OS << "\nstatic constexpr ImpliedExtsEntry ImpliedExts[] = {\n";
    for (const Record *Ext : Extensions) {
      auto ImpliesList = Ext->getValueAsListOfDefs("Implies");
      if (ImpliesList.empty())
        continue;

      StringRef Name = getExtensionName(Ext);

      for (auto *ImpliedExt : ImpliesList) {
        if (!ImpliedExt->isSubClassOf("RISCVExtension"))
          continue;

        OS.indent(4) << "{ {\"" << Name << "\"}, \""
                     << getExtensionName(ImpliedExt) << "\"},\n";
      }
    }

    OS << "};\n\n";
  }

  OS << "#endif // GET_IMPLIED_EXTENSIONS\n\n";
}

// We can generate march string from target features as what has been described
// in RISC-V ISA specification (version 20191213) 'Chapter 27. ISA Extension
// Naming Conventions'.
//
// This is almost the same as RISCVFeatures::parseFeatureBits, except that we
// get feature name from feature records instead of feature bits.
static void printMArch(raw_ostream &OS, ArrayRef<const Record *> Features) {
  RISCVISAUtils::OrderedExtensionMap Extensions;
  unsigned XLen = 0;

  // Convert features to FeatureVector.
  for (const Record *Feature : Features) {
    StringRef FeatureName = getExtensionName(Feature);
    if (Feature->isSubClassOf("RISCVExtension")) {
      unsigned Major = Feature->getValueAsInt("MajorVersion");
      unsigned Minor = Feature->getValueAsInt("MinorVersion");
      Extensions[FeatureName.str()] = {Major, Minor};
    } else if (FeatureName == "64bit") {
      assert(XLen == 0 && "Already determined XLen");
      XLen = 64;
    } else if (FeatureName == "32bit") {
      assert(XLen == 0 && "Already determined XLen");
      XLen = 32;
    }
  }

  assert(XLen != 0 && "Unable to determine XLen");

  OS << "rv" << XLen;

  ListSeparator LS("_");
  for (auto const &Ext : Extensions)
    OS << LS << Ext.first << Ext.second.Major << 'p' << Ext.second.Minor;
}

static void printProfileTable(raw_ostream &OS,
                              ArrayRef<const Record *> Profiles,
                              bool Experimental) {
  OS << "static constexpr RISCVProfile Supported";
  if (Experimental)
    OS << "Experimental";
  OS << "Profiles[] = {\n";

  for (const Record *Rec : Profiles) {
    if (Rec->getValueAsBit("Experimental") != Experimental)
      continue;

    StringRef Name = Rec->getValueAsString("Name");
    Name.consume_front("experimental-");
    OS.indent(4) << "{\"" << Name << "\",\"";
    printMArch(OS, Rec->getValueAsListOfDefs("Implies"));
    OS << "\"},\n";
  }

  OS << "};\n\n";
}

static void emitRISCVProfiles(const RecordKeeper &Records, raw_ostream &OS) {
  OS << "#ifdef GET_SUPPORTED_PROFILES\n";
  OS << "#undef GET_SUPPORTED_PROFILES\n\n";

  auto Profiles = Records.getAllDerivedDefinitionsIfDefined("RISCVProfile");

  if (!Profiles.empty()) {
    printProfileTable(OS, Profiles, /*Experimental=*/false);
    bool HasExperimentalProfiles = any_of(Profiles, [&](auto &Rec) {
      return Rec->getValueAsBit("Experimental");
    });
    if (HasExperimentalProfiles)
      printProfileTable(OS, Profiles, /*Experimental=*/true);
  }

  OS << "#endif // GET_SUPPORTED_PROFILES\n\n";
}

static void emitRISCVProcs(const RecordKeeper &RK, raw_ostream &OS) {
  OS << "#ifndef PROC\n"
     << "#define PROC(ENUM, NAME, DEFAULT_MARCH, FAST_SCALAR_UNALIGN"
     << ", FAST_VECTOR_UNALIGN, MVENDORID, MARCHID, MIMPID)\n"
     << "#endif\n\n";

  // Iterate on all definition records.
  for (const Record *Rec :
       RK.getAllDerivedDefinitionsIfDefined("RISCVProcessorModel")) {
    const std::vector<const Record *> &Features =
        Rec->getValueAsListOfDefs("Features");
    bool FastScalarUnalignedAccess = any_of(Features, [&](auto &Feature) {
      return Feature->getValueAsString("Name") == "unaligned-scalar-mem";
    });

    bool FastVectorUnalignedAccess = any_of(Features, [&](auto &Feature) {
      return Feature->getValueAsString("Name") == "unaligned-vector-mem";
    });

    OS << "PROC(" << Rec->getName() << ", {\"" << Rec->getValueAsString("Name")
       << "\"}, {\"";

    StringRef MArch = Rec->getValueAsString("DefaultMarch");

    // Compute MArch from features if we don't specify it.
    if (MArch.empty())
      printMArch(OS, Features);
    else
      OS << MArch;

    uint32_t MVendorID = Rec->getValueAsInt("MVendorID");
    uint64_t MArchID = Rec->getValueAsInt("MArchID");
    uint64_t MImpID = Rec->getValueAsInt("MImpID");

    OS << "\"}, " << FastScalarUnalignedAccess << ", "
       << FastVectorUnalignedAccess;
    OS << ", " << format_hex(MVendorID, 10);
    OS << ", " << format_hex(MArchID, 18);
    OS << ", " << format_hex(MImpID, 18);
    OS << ")\n";
  }
  OS << "\n#undef PROC\n";
  OS << "\n";
  OS << "#ifndef TUNE_PROC\n"
     << "#define TUNE_PROC(ENUM, NAME)\n"
     << "#endif\n\n";

  for (const Record *Rec :
       RK.getAllDerivedDefinitionsIfDefined("RISCVTuneProcessorModel")) {
    OS << "TUNE_PROC(" << Rec->getName() << ", "
       << "\"" << Rec->getValueAsString("Name") << "\")\n";
  }

  OS << "\n#undef TUNE_PROC\n";
}

static void emitRISCVExtensionBitmask(const RecordKeeper &RK, raw_ostream &OS) {
  std::vector<const Record *> Extensions =
      RK.getAllDerivedDefinitionsIfDefined("RISCVExtensionBitmask");
  llvm::sort(Extensions, [](const Record *Rec1, const Record *Rec2) {
    unsigned GroupID1 = Rec1->getValueAsInt("GroupID");
    unsigned GroupID2 = Rec2->getValueAsInt("GroupID");
    if (GroupID1 != GroupID2)
      return GroupID1 < GroupID2;

    return Rec1->getValueAsInt("BitPos") < Rec2->getValueAsInt("BitPos");
  });

#ifndef NDEBUG
  llvm::DenseSet<std::pair<uint64_t, uint64_t>> Seen;
#endif

  OS << "#ifdef GET_RISCVExtensionBitmaskTable_IMPL\n";
  OS << "static const RISCVExtensionBitmask ExtensionBitmask[]={\n";
  for (const Record *Rec : Extensions) {
    unsigned GroupIDVal = Rec->getValueAsInt("GroupID");
    unsigned BitPosVal = Rec->getValueAsInt("BitPos");

    StringRef ExtName = Rec->getValueAsString("Name");
    ExtName.consume_front("experimental-");

#ifndef NDEBUG
    assert(Seen.insert({GroupIDVal, BitPosVal}).second && "duplicated bitmask");
#endif

    OS.indent(4) << "{"
                 << "\"" << ExtName << "\""
                 << ", " << GroupIDVal << ", " << BitPosVal << "ULL"
                 << "},\n";
  }
  OS << "};\n";
  OS << "#endif\n";
}

static void emitRiscvTargetDef(const RecordKeeper &RK, raw_ostream &OS) {
  emitRISCVExtensions(RK, OS);
  emitRISCVProfiles(RK, OS);
  emitRISCVProcs(RK, OS);
  emitRISCVExtensionBitmask(RK, OS);
}

static TableGen::Emitter::Opt X("gen-riscv-target-def", emitRiscvTargetDef,
                                "Generate the list of CPUs and extensions for "
                                "RISC-V");
