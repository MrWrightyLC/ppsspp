#include <algorithm>


#include "ext/imgui/imgui_internal.h"

#include "Common/StringUtils.h"
#include "Core/Config.h"
#include "Core/RetroAchievements.h"
#include "Core/Core.h"
#include "Core/Debugger/DebugInterface.h"
#include "Core/Debugger/DisassemblyManager.h"
#include "Core/Debugger/Breakpoints.h"
#include "Core/MIPS/MIPSDebugInterface.h"
#include "Core/MIPS/MIPSTables.h"
#include "Core/Debugger/SymbolMap.h"
#include "Core/MemMap.h"
#include "Core/HLE/HLE.h"
#include "Common/System/Request.h"

#include "Core/HLE/sceAtrac.h"
#include "Core/HLE/AtracCtx.h"

// Threads window
#include "Core/HLE/sceKernelThread.h"

// Callstack window
#include "Core/MIPS/MIPSStackWalk.h"

// GPU things
#include "GPU/Common/GPUDebugInterface.h"

#include "UI/ImDebugger/ImDebugger.h"
#include "UI/ImDebugger/ImGe.h"

void DrawRegisterView(MIPSDebugInterface *mipsDebug, bool *open) {
	if (!ImGui::Begin("Registers", open)) {
		ImGui::End();
		return;
	}

	if (ImGui::BeginTabBar("RegisterGroups", ImGuiTabBarFlags_None)) {
		if (ImGui::BeginTabItem("GPR")) {
			if (ImGui::BeginTable("gpr", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersH)) {
				ImGui::TableSetupColumn("regname", ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableSetupColumn("value_i", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableNextRow();

				auto gprLine = [&](const char *regname, int value) {
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("%s", regname);
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%08x", value);
					if (value >= -1000000 && value <= 1000000) {
						ImGui::TableSetColumnIndex(2);
						ImGui::Text("%d", value);
					}
					ImGui::TableNextRow();
				};
				for (int i = 0; i < 32; i++) {
					gprLine(mipsDebug->GetRegName(0, i).c_str(), mipsDebug->GetGPR32Value(i));
				}
				gprLine("hi", mipsDebug->GetHi());
				gprLine("lo", mipsDebug->GetLo());
				gprLine("pc", mipsDebug->GetPC());
				gprLine("ll", mipsDebug->GetLLBit());
				ImGui::EndTable();
			}

			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("FPU")) {
			if (ImGui::BeginTable("fpr", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersH)) {
				ImGui::TableSetupColumn("regname", ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthFixed);
				ImGui::TableSetupColumn("value_i", ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableNextRow();
				for (int i = 0; i < 32; i++) {
					float fvalue = mipsDebug->GetFPR32Value(i);
					u32 fivalue;
					memcpy(&fivalue, &fvalue, sizeof(fivalue));
					ImGui::TableSetColumnIndex(0);
					ImGui::Text("%s", mipsDebug->GetRegName(1, i).c_str());
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%0.7f", fvalue);
					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%08x", fivalue);
					ImGui::TableNextRow();
				}
				ImGui::EndTable();
			}
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("VFPU")) {
			ImGui::Text("TODO");
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}
	ImGui::End();
}

static const char *ThreadStatusToString(u32 status) {
	switch (status) {
	case THREADSTATUS_RUNNING: return "Running";
	case THREADSTATUS_READY: return "Ready";
	case THREADSTATUS_WAIT: return "Wait";
	case THREADSTATUS_SUSPEND: return "Suspended";
	case THREADSTATUS_DORMANT: return "Dormant";
	case THREADSTATUS_DEAD: return "Dead";
	case THREADSTATUS_WAITSUSPEND: return "WaitSuspended";
	default:
		break;
	}
	return "(unk)";
}

void DrawThreadView(ImConfig &cfg) {
	if (!ImGui::Begin("Threads", &cfg.threadsOpen)) {
		ImGui::End();
		return;
	}

	std::vector<DebugThreadInfo> info = GetThreadsInfo();
	if (ImGui::BeginTable("threads", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersH)) {
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("PC", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Entry", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Priority", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthStretch);

		ImGui::TableHeadersRow();

		for (int i = 0; i < (int)info.size(); i++) {
			const auto &thread = info[i];
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::PushID(i);
			if (ImGui::Selectable(thread.name, cfg.selectedThread == i, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns)) {
				cfg.selectedThread = i;
			}
			if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
				cfg.selectedThread = i;
				ImGui::OpenPopup("threadPopup");
			}
			ImGui::TableNextColumn();
			ImGui::Text("%08x", thread.curPC);
			ImGui::TableNextColumn();
			ImGui::Text("%08x", thread.entrypoint);
			ImGui::TableNextColumn();
			ImGui::Text("%d", thread.priority);
			ImGui::TableNextColumn();
			ImGui::Text("%s", ThreadStatusToString(thread.status));
			if (ImGui::BeginPopup("threadPopup")) {
				DebugThreadInfo &thread = info[i];
				ImGui::Text("Thread: %s", thread.name);
				if (ImGui::MenuItem("Kill thread")) {
					sceKernelTerminateThread(thread.id);
				}
				if (ImGui::MenuItem("Force run")) {
					__KernelResumeThreadFromWait(thread.id, 0);
				}
				ImGui::EndPopup();
			}
			ImGui::PopID();
		}

		ImGui::EndTable();
	}
	ImGui::End();
}

static const char *MemCheckConditionToString(MemCheckCondition cond) {
	switch (cond) {
	case MEMCHECK_READ: return "Read";
	case MEMCHECK_WRITE: return "Write";
	case MEMCHECK_READWRITE: return "Read/Write";
	case MEMCHECK_WRITE | MEMCHECK_WRITE_ONCHANGE: return "Write Change";
	case MEMCHECK_READWRITE | MEMCHECK_WRITE_ONCHANGE: return "Read/Write Change";
	default:
		return "(bad!)";
	}
}

static void DrawBreakpointsView(MIPSDebugInterface *mipsDebug, ImConfig &cfg) {
	if (!ImGui::Begin("Breakpoints", &cfg.breakpointsOpen)) {
		ImGui::End();
		return;
	}
	if (ImGui::BeginTable("bp_window", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
		ImGui::TableSetupColumn("left", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("right", ImGuiTableColumnFlags_WidthFixed, 150.0f);
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		auto &bps = g_breakpoints.GetBreakpointRefs();
		auto &mcs = g_breakpoints.GetMemCheckRefs();

		if (cfg.selectedBreakpoint >= bps.size()) {
			cfg.selectedBreakpoint = -1;
		}
		if (cfg.selectedMemCheck >= mcs.size()) {
			cfg.selectedMemCheck = -1;
		}

		if (ImGui::Button("Add Breakpoint")) {
			cfg.selectedBreakpoint = g_breakpoints.AddBreakPoint(0);
			cfg.selectedMemCheck = -1;
		}
		ImGui::SameLine();
		if (ImGui::Button("Add MemCheck")) {
			cfg.selectedMemCheck = g_breakpoints.AddMemCheck(0, 0, MemCheckCondition::MEMCHECK_WRITE, BreakAction::BREAK_ACTION_PAUSE);
			cfg.selectedBreakpoint = -1;
		}

		if (ImGui::BeginTable("breakpoints", 8, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersH)) {
			ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Size/Label", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("OpCode", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Cond", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("Hits", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableHeadersRow();

			for (int i = 0; i < (int)bps.size(); i++) {
				auto &bp = bps[i];
				if (bp.temporary) {
					// Probably won't happen a lot.. maybe a wrong Step Over
					continue;
				}
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::PushID(i);
				// TODO: This clashes with the checkbox!
				if (ImGui::Selectable("", cfg.selectedBreakpoint == i, ImGuiSelectableFlags_SpanAllColumns)) {
					cfg.selectedBreakpoint = i;
					cfg.selectedMemCheck = -1;
				}
				ImGui::SameLine();
				ImGui::CheckboxFlags("##enabled", (int *)&bp.result, (int)BREAK_ACTION_PAUSE);
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Exec");
				ImGui::TableNextColumn();
				ImGui::Text("%08x", bp.addr);
				ImGui::TableNextColumn();
				const std::string sym = g_symbolMap->GetLabelString(bp.addr);
				if (!sym.empty()) {
					ImGui::TextUnformatted(sym.c_str());  // size/label
				} else {
					ImGui::TextUnformatted("-");  // size/label
				}
				ImGui::TableNextColumn();
				// disasm->getOpcodeText(displayedBreakPoints_[index].addr, temp, sizeof(temp));
				ImGui::TextUnformatted("-");  // opcode
				ImGui::TableNextColumn();
				if (bp.hasCond) {
					ImGui::Text("%s", bp.cond.expressionString.c_str());
				} else {
					ImGui::TextUnformatted("-");  // condition
				}
				ImGui::TableNextColumn();
				ImGui::Text("-");  // hits not available on exec bps yet
				ImGui::PopID();
			}

			// OK, now list the memchecks.
			for (int i = 0; i < (int)mcs.size(); i++) {
				auto &mc = mcs[i];
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::PushID(i + 10000);
				if (ImGui::Selectable("", cfg.selectedMemCheck == i, ImGuiSelectableFlags_SpanAllColumns)) {
					cfg.selectedBreakpoint = -1;
					cfg.selectedMemCheck = i;
				}
				ImGui::SameLine();
				ImGui::CheckboxFlags("", (int *)&mc.result, BREAK_ACTION_PAUSE);
				ImGui::TableNextColumn();
				ImGui::Text("%s", MemCheckConditionToString(mc.cond));
				ImGui::TableNextColumn();
				ImGui::Text("%08x", mc.start);
				ImGui::TableNextColumn();
				ImGui::Text("%08x", mc.end ? (mc.end - mc.start) : 1);
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("-");  // opcode
				ImGui::TableNextColumn();
				ImGui::TextUnformatted("-");  // cond
				ImGui::TableNextColumn();
				ImGui::Text("%d", mc.numHits);
				ImGui::PopID();
			}

			ImGui::EndTable();
		}

		ImGui::TableNextColumn();

		if (cfg.selectedBreakpoint >= 0) {
			// Add edit form for breakpoints
			if (ImGui::BeginChild("bp_edit")) {
				auto &bp = bps[cfg.selectedBreakpoint];
				ImGui::TextUnformatted("Edit breakpoint");
				ImGui::CheckboxFlags("Enabled", (int *)&bp.result, (int)BREAK_ACTION_PAUSE);
				ImGui::InputScalar("Address", ImGuiDataType_U32, &bp.addr, nullptr, nullptr, "%08x", ImGuiInputTextFlags_CharsHexadecimal);
				if (ImGui::Button("Delete")) {
					g_breakpoints.RemoveBreakPoint(bp.addr);
				}
				ImGui::EndChild();
			}
		}

		if (cfg.selectedMemCheck >= 0) {
			// Add edit form for memchecks
			if (ImGui::BeginChild("mc_edit")) {
				auto &mc = mcs[cfg.selectedMemCheck];
				ImGui::TextUnformatted("Edit memcheck");
				ImGui::CheckboxFlags("Enabled", (int *)&mc.result, (int)BREAK_ACTION_PAUSE);
				ImGui::InputScalar("Start", ImGuiDataType_U32, &mc.start);
				ImGui::InputScalar("End", ImGuiDataType_U32, &mc.end);
				if (ImGui::Button("Delete")) {
					g_breakpoints.RemoveMemCheck(mcs[cfg.selectedMemCheck].start, mcs[cfg.selectedMemCheck].end);
				}
				ImGui::EndChild();
			}
		}
		ImGui::EndTable();
	}

	ImGui::End();
}

void DrawAtracView(ImConfig &cfg) {
	if (!ImGui::Begin("sceAtrac contexts", &cfg.atracOpen)) {
		ImGui::End();
		return;
	}

	if (ImGui::BeginTable("atracs", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersH)) {
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("OutChans", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("CurrentSample", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("RemainingFrames", ImGuiTableColumnFlags_WidthFixed);

		ImGui::TableHeadersRow();

		for (int i = 0; i < PSP_NUM_ATRAC_IDS; i++) {
			u32 type = 0;
			const AtracBase *atracBase = __AtracGetCtx(i, &type);
			ImGui::TableNextRow();
			ImGui::TableNextColumn();

			if (!atracBase) {
				ImGui::Text("-", type);
				continue;
			}

			ImGui::Text("%d", type);
			ImGui::TableNextColumn();
			ImGui::Text("%d", atracBase->GetOutputChannels());
			ImGui::TableNextColumn();
			ImGui::Text("%d", atracBase->CurrentSample());
			ImGui::TableNextColumn();
			ImGui::Text("%d", atracBase->RemainingFrames());
		}

		ImGui::EndTable();
	}
	ImGui::End();
}

void DrawCallStacks(MIPSDebugInterface *debug, bool *open) {
	if (!ImGui::Begin("Callstacks", open)) {
		ImGui::End();
		return;
	}

	std::vector<DebugThreadInfo> info = GetThreadsInfo();
	// TODO: Add dropdown for thread choice.
	u32 entry = 0;
	u32 stackTop = 0;
	for (auto &thread : info) {
		if (thread.isCurrent) {
			entry = thread.entrypoint;
			stackTop = thread.initialStack;
			break;
		}
	}

	if (entry != 0 && ImGui::BeginTable("frames", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersH)) {
		ImGui::TableSetupColumn("Entry", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("EntryAddr", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("CurPC", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("CurOpCode", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("CurSP", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthStretch);

		ImGui::TableHeadersRow();
		ImGui::TableNextRow();

		std::vector<MIPSStackWalk::StackFrame> frames = MIPSStackWalk::Walk(debug->GetPC(), debug->GetRegValue(0, 31), debug->GetRegValue(0, 29), entry, stackTop);

		// TODO: Add context menu and clickability
		for (auto &frame : frames) {
			const std::string entrySym = g_symbolMap->GetLabelString(frame.entry);

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%s", entrySym.c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%08x", frame.entry);
			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%08x", frame.pc);
			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%s", "N/A");  // opcode, see the old debugger
			ImGui::TableSetColumnIndex(4);
			ImGui::Text("%08x", frame.sp);
			ImGui::TableSetColumnIndex(5);
			ImGui::Text("%d", frame.stackSize);
			ImGui::TableNextRow();
			// TODO: More fields?
		}

		ImGui::EndTable();
	}
	ImGui::End();
}

void DrawModules(MIPSDebugInterface *debug, ImConfig &cfg) {
	if (!ImGui::Begin("Modules", &cfg.modulesOpen) || !g_symbolMap) {
		ImGui::End();
		return;
	}

	std::vector<LoadedModuleInfo> modules = g_symbolMap->getAllModules();
	if (ImGui::BeginTable("modules", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersH)) {
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Active", ImGuiTableColumnFlags_WidthFixed);

		ImGui::TableHeadersRow();

		// TODO: Add context menu and clickability
		for (int i = 0; i < (int)modules.size(); i++) {
			auto &module = modules[i];
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			if (ImGui::Selectable(module.name.c_str(), cfg.selectedModule == i, ImGuiSelectableFlags_SpanAllColumns)) {
				cfg.selectedModule = i;
			}
			ImGui::TableNextColumn();
			ImGui::Text("%08x", module.address);
			ImGui::TableNextColumn();
			ImGui::Text("%08x", module.size);
			ImGui::TableNextColumn();
			ImGui::Text("%s", module.active ? "yes" : "no");
		}

		ImGui::EndTable();
	}

	if (cfg.selectedModule >= 0 && cfg.selectedModule < (int)modules.size()) {
		auto &module = modules[cfg.selectedModule];
		// TODO: Show details
	}
	ImGui::End();
}

void DrawHLEModules(ImConfig &config) {
	if (!ImGui::Begin("HLE Modules", &config.hleModulesOpen)) {
		ImGui::End();
		return;
	}

	const int moduleCount = GetNumRegisteredModules();
	std::vector<const HLEModule *> modules;
	modules.reserve(moduleCount);
	for (int i = 0; i < moduleCount; i++) {
		modules.push_back(GetModuleByIndex(i));
	}

	std::sort(modules.begin(), modules.end(), [](const HLEModule* a, const HLEModule* b) {
		return std::strcmp(a->name, b->name) < 0;
	});

	for (auto mod : modules) {
		if (ImGui::TreeNode(mod->name)) {
			for (int j = 0; j < mod->numFunctions; j++) {
				auto &func = mod->funcTable[j];
				ImGui::Text("%s(%s)", func.name, func.argmask);
			}
			ImGui::TreePop();
		}
	}

	ImGui::End();
}

void ImDebugger::Frame(MIPSDebugInterface *mipsDebug, GPUDebugInterface *gpuDebug) {
	// Snapshot the coreState to avoid inconsistency.
	const CoreState coreState = ::coreState;

	if (Achievements::HardcoreModeActive()) {
		ImGui::Begin("RetroAchievements hardcore mode");
		ImGui::Text("The debugger may not be used when the\nRetroAchievements hardcore mode is enabled.");
		ImGui::Text("To use the debugger, go into Settings / Tools / RetroAchievements and disable them.");
		ImGui::End();
		return;
	}

	if (ImGui::BeginMainMenuBar()) {
		if (ImGui::BeginMenu("Debug")) {
			if (coreState == CoreState::CORE_STEPPING) {
				if (ImGui::MenuItem("Run")) {
					Core_Resume();
				}
				if (ImGui::MenuItem("Step Into", "F11")) {
					Core_RequestSingleStep(CPUStepType::Into, 1);
				}
				if (ImGui::MenuItem("Step Over", "F10")) {
					Core_RequestSingleStep(CPUStepType::Over, 1);
				}
				if (ImGui::MenuItem("Step Out", "Shift+F11")) {
					Core_RequestSingleStep(CPUStepType::Out, 1);
				}
			} else {
				if (ImGui::MenuItem("Break")) {
					Core_Break("Menu:Break");
				}
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("CPU")) {
			ImGui::MenuItem("CPU debugger", nullptr, &cfg_.disasmOpen);
			ImGui::MenuItem("Registers", nullptr, &cfg_.regsOpen);
			ImGui::MenuItem("Callstacks", nullptr, &cfg_.callstackOpen);
			ImGui::MenuItem("Breakpoints", nullptr, &cfg_.breakpointsOpen);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("OS HLE")) {
			ImGui::MenuItem("HLE Threads", nullptr, &cfg_.threadsOpen);
			ImGui::MenuItem("HLE Modules",nullptr,  &cfg_.modulesOpen);
			ImGui::MenuItem("sceAtrac", nullptr, &cfg_.atracOpen);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Ge (GPU)")) {
			ImGui::MenuItem("Framebuffers", nullptr, &cfg_.framebuffersOpen);
			// More to come here...
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Tools")) {
			ImGui::MenuItem("Struct viewer", nullptr, &cfg_.structViewerOpen);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Misc")) {
			if (ImGui::MenuItem("Close Debugger")) {
				g_Config.bShowImDebugger = false;
			}
			ImGui::MenuItem("Dear ImGui Demo", nullptr, &cfg_.demoOpen);
			ImGui::MenuItem("Dear ImGui Style editor", nullptr, &cfg_.styleEditorOpen);
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}

	if (cfg_.demoOpen) {
		ImGui::ShowDemoWindow(&cfg_.demoOpen);
	}

	if (cfg_.styleEditorOpen) {
		ImGui::ShowStyleEditor();
	}

	if (cfg_.disasmOpen) {
		disasm_.Draw(mipsDebug, &cfg_.disasmOpen, coreState);
	}

	if (cfg_.regsOpen) {
		DrawRegisterView(mipsDebug, &cfg_.regsOpen);
	}

	if (cfg_.breakpointsOpen) {
		DrawBreakpointsView(mipsDebug, cfg_);
	}

	if (cfg_.threadsOpen) {
		DrawThreadView(cfg_);
	}

	if (cfg_.callstackOpen) {
		DrawCallStacks(mipsDebug, &cfg_.callstackOpen);
	}

	if (cfg_.modulesOpen) {
		DrawModules(mipsDebug, cfg_);
	}

	if (cfg_.atracOpen) {
		DrawAtracView(cfg_);
	}

	if (cfg_.hleModulesOpen) {
		DrawHLEModules(cfg_);
	}

	if (cfg_.framebuffersOpen) {
		DrawFramebuffersWindow(cfg_, gpuDebug->GetFramebufferManagerCommon());
	}

	if (cfg_.structViewerOpen) {
		structViewer_.Draw(mipsDebug, &cfg_.structViewerOpen);
	}
}

void ImDisasmWindow::Draw(MIPSDebugInterface *mipsDebug, bool *open, CoreState coreState) {
	char title[256];
	snprintf(title, sizeof(title), "%s - Disassembly", "Allegrex MIPS");

	disasmView_.setDebugger(mipsDebug);

	ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(title, open, ImGuiWindowFlags_NoNavInputs)) {
		ImGui::End();
		return;
	}

	if (ImGui::IsWindowFocused()) {
		// Process stepping keyboard shortcuts.
		if (ImGui::IsKeyPressed(ImGuiKey_F10)) {
			Core_RequestSingleStep(CPUStepType::Over, 0);
		}
		if (ImGui::IsKeyPressed(ImGuiKey_F11)) {
			Core_RequestSingleStep(CPUStepType::Into, 0);
		}
	}

	ImGui::BeginDisabled(coreState != CORE_STEPPING);
	if (ImGui::SmallButton("Run")) {
		Core_Resume();
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::BeginDisabled(coreState != CORE_RUNNING);
	if (ImGui::SmallButton("Pause")) {
		Core_Break("Pause");
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	if (ImGui::SmallButton("Step Into")) {
		u32 stepSize = disasmView_.getInstructionSizeAt(mipsDebug->GetPC());
		Core_RequestSingleStep(CPUStepType::Into, stepSize);
	}

	ImGui::SameLine();
	if (ImGui::SmallButton("Step Over")) {
		Core_RequestSingleStep(CPUStepType::Over, 0);
	}

	ImGui::SameLine();
	if (ImGui::SmallButton("Step Out")) {
		Core_RequestSingleStep(CPUStepType::Out, 0);
	}

	ImGui::SameLine();
	if (ImGui::SmallButton("Goto PC")) {
		disasmView_.gotoPC();
	}

	if (ImGui::BeginPopup("disSearch")) {
		if (ImGui::InputText("Search", searchTerm_, sizeof(searchTerm_), ImGuiInputTextFlags_EnterReturnsTrue)) {
			disasmView_.Search(searchTerm_);
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	ImGui::SameLine();
	if (ImGui::SmallButton("Search")) {
		// Open a small popup
		ImGui::OpenPopup("disSearch");
		ImGui::Shortcut(ImGuiKey_F | ImGuiMod_Ctrl);
	}

	ImGui::SameLine();
	if (ImGui::SmallButton("Next")) {
		disasmView_.SearchNext(true);
	}

	ImGui::SameLine();
	ImGui::Checkbox("Follow PC", &disasmView_.followPC_);

	ImGui::SetNextItemWidth(100);
	if (ImGui::InputScalar("Go to addr: ", ImGuiDataType_U32, &gotoAddr_, NULL, NULL, "%08X", ImGuiInputTextFlags_EnterReturnsTrue)) {
		disasmView_.setCurAddress(gotoAddr_);
		disasmView_.scrollAddressIntoView();
	}

	if (ImGui::BeginTable("main", 2)) {
		ImGui::TableSetupColumn("left", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("right", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);

		if (symCache_.empty() || symsDirty_) {
			symCache_ = g_symbolMap->GetAllSymbols(SymbolType::ST_FUNCTION);
			symsDirty_ = false;
		}

		if (selectedSymbol_ >= 0 && selectedSymbol_ < symCache_.size()) {
			auto &sym = symCache_[selectedSymbol_];
			if (ImGui::TreeNode("Edit Symbol", "Edit %s", sym.name.c_str())) {
				if (ImGui::InputText("Name", selectedSymbolName_, sizeof(selectedSymbolName_), ImGuiInputTextFlags_EnterReturnsTrue)) {
					g_symbolMap->SetLabelName(selectedSymbolName_, sym.address);
					symsDirty_ = true;
				}
				ImGui::Text("%08x (size: %0d)", sym.address, sym.size);
				ImGui::TreePop();
			}
		}

		ImVec2 sz = ImGui::GetContentRegionAvail();
		if (ImGui::BeginListBox("##symbols", ImVec2(150.0, sz.y - ImGui::GetTextLineHeightWithSpacing() * 2))) {
			ImGuiListClipper clipper;
			clipper.Begin((int)symCache_.size(), -1);
			while (clipper.Step()) {
				for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
					if (ImGui::Selectable(symCache_[i].name.c_str(), selectedSymbol_ == i)) {
						disasmView_.gotoAddr(symCache_[i].address);
						disasmView_.scrollAddressIntoView();
						truncate_cpy(selectedSymbolName_, symCache_[i].name.c_str());
						selectedSymbol_ = i;
					}
				}
			}
			clipper.End();
			ImGui::EndListBox();
		}

		ImGui::TableSetColumnIndex(1);
		disasmView_.Draw(ImGui::GetWindowDrawList());
		ImGui::EndTable();

		ImGui::Text("%s", disasmView_.StatusBarText().c_str());
	}
	ImGui::End();
}
