-- DESCRIPTION: Verilator: VHDL architecture function sizing a subtype
--
-- "signed(PIPELINE_BITS-1 downto 0)" calls a function the architecture
-- declares, whose body reads a constant worked out from a generic. The
-- function is a constant function there, so the constant it reads is
-- settled by elaboration too. A helper made for a slice whose bound calls
-- the function stays in the module beside it (GHDL gna issue2065).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use ieee.math_real.all;

entity t_vhdl_constfuncread is
  generic (N : natural := 8; BITS : natural := 16);
end entity;

architecture arch of t_vhdl_constfuncread is
  constant NUM_STAGES : integer := integer(ceil(log2(real(N))));
  function PIPELINE_BITS return integer is
  begin
    return BITS + NUM_STAGES;
  end function;
  constant DATA_BITS : integer := PIPELINE_BITS * 2;
  type cs_t is record
    i : signed(PIPELINE_BITS-1 downto 0);
    q : signed(PIPELINE_BITS-1 downto 0);
  end record;
  function slv_to_cst(x : std_logic_vector) return cs_t is
    variable ret : cs_t;
  begin
    ret.i := resize(signed(x(x'high downto PIPELINE_BITS)), PIPELINE_BITS);
    ret.q := resize(signed(x(PIPELINE_BITS-1 downto 0)), PIPELINE_BITS);
    return ret;
  end function;
  signal data : std_logic_vector(DATA_BITS-1 downto 0) := (others => '1');
  signal it : natural := 7;
  signal st : natural := 1;
  signal tw : unsigned(NUM_STAGES-1 downto 0);
begin
  process
    variable c : cs_t;
    variable ones_reg : unsigned(NUM_STAGES-1 downto 0);
  begin
    assert c.i'length = 19 severity failure;
    data(PIPELINE_BITS-1 downto 0) <= std_logic_vector(to_signed(5, PIPELINE_BITS));
    ones_reg := (others => '1');
    wait for 1 ns;
    c := slv_to_cst(data);
    assert c.q = 5 severity failure;
    assert c.i = -1 severity failure;
    -- A carried value and the shift of a vector whose length is not
    tw <= to_unsigned(it, ones_reg'high + 1) and shift_left(ones_reg, NUM_STAGES - 1 - st);
    wait for 1 ns;
    assert tw = "110" severity failure;
    report "constfuncread done";
    std.env.finish;
  end process;
end architecture;
