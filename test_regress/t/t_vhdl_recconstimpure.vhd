-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

-- An architecture's constant of a record of integers and a boolean, given
-- what an impure function returns from a generic of that record type: its
-- value is worked out as the design starts, so it stays an object given its
-- value once, not a parameter. Shaped after VUnit's bus2memory, "constant
-- my_memory : memory_t := to_vc_interface(memory)".

package t_vhdl_recconstimpure_pkg is
  type RecType is record
    a : integer;
    b : boolean;
    c : natural;
  end record;
  constant REC_DEFAULT_C : RecType := (a => 5, b => false, c => 7);
end package;

use work.t_vhdl_recconstimpure_pkg.all;

entity t_vhdl_recconstimpure is
  generic (N_G : natural := 5; MEM_G : RecType := REC_DEFAULT_C);
end entity;

architecture rtl of t_vhdl_recconstimpure is
  type IntPtr is access integer;
  impure function mk (n : natural) return RecType is
    variable p : IntPtr := new integer'(n);
    variable r : RecType;
  begin
    r := (a => p.all + 1, b => n > 3, c => 2 * p.all);
    deallocate(p);
    return r;
  end function;
  impure function touch (g : RecType; other : RecType := REC_DEFAULT_C) return RecType is
    variable result : RecType := g;
  begin
    if other /= REC_DEFAULT_C then
      result.a := other.a;
    end if;
    result.b := true;
    return result;
  end function;
  constant MY_C : RecType := mk(N_G);
  constant MEM_C : RecType := touch(MEM_G);
  constant PLAIN_C : RecType := (a => 1, b => false, c => 2);
begin
  process
  begin
    assert MY_C.a = 6 and MY_C.b and MY_C.c = 10 report "my" severity failure;
    assert PLAIN_C.a = 1 and not PLAIN_C.b report "plain" severity failure;
    assert MEM_C.a = 5 and MEM_C.b and MEM_C.c = 7 report "mem" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
