-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A component's generic and the entity's are two declarations of the same
-- name (7.3.2.2), and an instance names the component's.  Where the value
-- an instance gives is one no elaboration can fold -- an element of a
-- deferred constant a function builds -- the generic has to travel as data
-- rather than as a parameter, and both declarations have to agree on that.

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

package pk is
  constant BW : integer := 8;
  type sig_array is array (natural range <>) of signed(BW - 1 downto 0);
  -- Deferred: its value is a call, which only the package body makes
  constant INIT : sig_array(0 to 3);
  component acs is
    generic ( INITIALIZE_VALUE : in signed(BW - 1 downto 0) );
    port ( clk : in std_logic; q : out signed(BW - 1 downto 0) );
  end component acs;
end package;

package body pk is
  function calc_init return sig_array is
    variable v : sig_array(0 to 3);
  begin
    for i in 0 to 3 loop
      v(i) := to_signed(i * 3 + 1, BW);
    end loop;
    return v;
  end function;
  constant INIT : sig_array(0 to 3) := calc_init;
end package body;

-------------------------------------------------------------------------------

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.pk.all;

entity acs is
  generic ( INITIALIZE_VALUE : in signed(BW - 1 downto 0) );
  port ( clk : in std_logic; q : out signed(BW - 1 downto 0) );
end entity;

architecture rtl of acs is
begin
  q <= INITIALIZE_VALUE when rising_edge(clk);
end architecture;

-------------------------------------------------------------------------------

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;
use work.pk.all;
use std.env.all;

entity t_vhdl_compgen is
end entity;

architecture tb of t_vhdl_compgen is
  signal clk : std_logic := '0';
  type qarr is array (0 to 3) of signed(BW - 1 downto 0);
  signal qs : qarr;
begin

  g : for i in 0 to 3 generate
  begin
    inst : acs
      generic map ( INITIALIZE_VALUE => INIT(i) )
      port map ( clk => clk, q => qs(i) );
  end generate;

  main : process is
  begin
    wait for 1 ns;
    clk <= '1';
    wait for 1 ns;
    for i in 0 to 3 loop
      assert qs(i) = to_signed(i * 3 + 1, BW)
        report "instance " & integer'image(i) & " took the wrong generic value"
        severity failure;
    end loop;
    report "*-* All Finished *-*";
    finish;
  end process;

end architecture;
