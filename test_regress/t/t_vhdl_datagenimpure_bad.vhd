-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--

-- A generate condition written over a generic an impure function builds --
-- a record holding a number a protected counter handed out, as VUnit's
-- verification components compare their actors -- needs that value while
-- elaborating. Here the counter is one elaboration does not follow, so only
-- running the design gives the number, and the condition turns on nothing
-- else. It is refused rather than decided on a default (nvc 1.22.1
-- generates the leaf).

package t_vhdl_datagenimpure_bad_pkg is
  type leaf_t is record
    id : integer;
    len : natural;
  end record;
  constant null_leaf : leaf_t := (id => -1, len => 5);
  type outer_t is record
    p_leaf : leaf_t;
    width : natural;
  end record;
  type counter_t is protected
    impure function next_id return integer;
  end protected;
  impure function new_leaf(len : natural) return leaf_t;
  impure function new_outer(leaf : leaf_t) return outer_t;
end package;
package body t_vhdl_datagenimpure_bad_pkg is
  type counter_t is protected body
    variable n : integer := 0;
    impure function next_id return integer is
    begin
      -- A labelled exit is not followed while elaborating
      outer : for i in 0 to 1 loop
        for j in 0 to 1 loop
          exit outer when j = 1;
          n := n + 1;
        end loop;
      end loop;
      return n;
    end function;
  end protected body;
  shared variable counter : counter_t;
  impure function new_leaf(len : natural) return leaf_t is
  begin
    return (id => counter.next_id, len => len);
  end function;
  impure function new_outer(leaf : leaf_t) return outer_t is
  begin
    return (p_leaf => leaf, width => 8);
  end function;
end package body;

use work.t_vhdl_datagenimpure_bad_pkg.all;
entity t_vhdl_datagenimpure_bad_leaf is
  generic (g : leaf_t);
  port (o : out integer);
end entity;
architecture a of t_vhdl_datagenimpure_bad_leaf is
begin
  o <= g.id * 100 + g.len;
end architecture;

use work.t_vhdl_datagenimpure_bad_pkg.all;
entity t_vhdl_datagenimpure_bad_mid is
  generic (m : outer_t);
  port (o : out integer);
end entity;
architecture a of t_vhdl_datagenimpure_bad_mid is
begin
  gen : if m.p_leaf /= null_leaf generate
    leaf_inst : entity work.t_vhdl_datagenimpure_bad_leaf
      generic map (g => m.p_leaf)
      port map (o => o);
  end generate;
end architecture;

use work.t_vhdl_datagenimpure_bad_pkg.all;
entity t_vhdl_datagenimpure_bad is
end entity;
architecture a of t_vhdl_datagenimpure_bad is
  constant c : outer_t := new_outer(new_leaf(5));
  signal o : integer;
begin
  mid_inst : entity work.t_vhdl_datagenimpure_bad_mid generic map (m => c) port map (o => o);
  process
  begin
    wait for 1 ns;
    report "o=" & integer'image(o);
    std.env.finish;
  end process;
end architecture;
