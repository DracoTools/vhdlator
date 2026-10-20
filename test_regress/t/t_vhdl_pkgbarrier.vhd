-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A barrier that is an element of a package's record, named through an
-- alias: OSVVM's "alias OsvvmTestDone : BarrierType is Barrier.TestDone"
-- in TbUtilPkg, which the Wishbone tests' TestCtrl processes wait at. The
-- processes were not counted as drivers of the record, so the first to
-- arrive wrote it alone and passed straight through, ending the test.

package t_vhdl_pkgbarrier_pkg is
  function resolved_barrier(s : integer_vector) return integer;
  subtype BarrierType is resolved_barrier integer range integer'low + 1 to integer'high;
  procedure WaitForBarrier(signal Sig : inout BarrierType);
  type PredefinedBarrierType is record
    ResetDone : BarrierType;
    TestDone  : BarrierType;
  end record;
  signal Barrier : PredefinedBarrierType;
  alias OsvvmTestDone : BarrierType is Barrier.TestDone;
  alias TestDone : BarrierType is Barrier.TestDone;
end package;
package body t_vhdl_pkgbarrier_pkg is
  function resolved_barrier(s : integer_vector) return integer is
    variable result : integer := 0;
  begin
    for i in s'range loop
      if s(i) /= 0 then
        result := result + 1;
      end if;
    end loop;
    return result;
  end function;
  procedure WaitForBarrier(signal Sig : inout BarrierType) is
  begin
    Sig <= 0;
    wait until Sig = 0;
    Sig <= 1;
  end procedure;
end package body;

use work.t_vhdl_pkgbarrier_pkg.all;
entity t_vhdl_pkgbarrier is end;
architecture t of t_vhdl_pkgbarrier is
begin
  ctl : process
  begin
    wait for 1 ns;
    report "before " & integer'image(OsvvmTestDone);
    WaitForBarrier(OsvvmTestDone);
    report "released at " & time'image(now);
    assert now = 50 ns report "released early" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
  mgr : process
  begin
    wait for 30 ns;
    WaitForBarrier(OsvvmTestDone);
    wait;
  end process;
  -- A third party, through a second alias of the same element (TbUtilPkg's TestDone)
  mem : process
  begin
    wait for 50 ns;
    WaitForBarrier(TestDone);
    wait;
  end process;
end;
