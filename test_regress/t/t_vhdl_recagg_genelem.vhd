-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A record aggregate assigned to a formal whose record elements the actual
-- constrains by generics: the value a call gives an element is made as long
-- as that element (GHDL gna issue3007).

entity t_vhdl_recagg_genelem is
  generic (W1 : positive := 12; W2 : natural := 4);
end entity;

architecture behav of t_vhdl_recagg_genelem is
  type sub_rec_t is record
    arg1 : bit_vector;
    arg2 : bit_vector;
  end record;
  type rec_t is record
    sub : sub_rec_t;
  end record;

  pure function ones (size : natural) return bit_vector is
  begin
    return (size - 1 downto 0 => '1');
  end function;

  procedure assign (r : out rec_t) is
    constant c1 : bit_vector(W1 - 1 downto 0) := (others => '0');
  begin
    r.sub := (arg1 => c1, arg2 => ones(W2));
  end procedure;
begin
  process
    variable rec : rec_t(sub(arg1(W1 - 1 downto 0), arg2(W2 - 1 downto 0)));
  begin
    assign(rec);
    assert rec.sub.arg2 = "1111" and rec.sub.arg1 = x"000" severity failure;
    report "*-* All Finished *-*";
    std.env.finish;
  end process;
end architecture;
