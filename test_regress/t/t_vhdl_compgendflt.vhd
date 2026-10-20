-- DESCRIPTION: Verilator: VHDL Test module
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0
--
-- A component's generic whose default names an earlier generic of the
-- component, "tpd : time := DEFAULT_DELAY" as in OSVVM's DpRam and Ethernet
-- components: an instance that leaves it out gives the entity the default
-- worked out from what the instance gives the earlier one (6.5.6.2, 7.3.3).
-- The entity's own default is not the component's.

entity t_vhdl_compgendflt_sub is
  generic (DEFAULT_DELAY : time := 2 ns; tpd : time := 7 ns; id : integer := 0);
end entity;

architecture a of t_vhdl_compgendflt_sub is
begin
  process
  begin
    report integer'image(id) & ": DEFAULT_DELAY=" & time'image(DEFAULT_DELAY)
      & " tpd=" & time'image(tpd);
    case id is
      when 1 => assert DEFAULT_DELAY = 1 ns and tpd = 1 ns severity failure;
      when 2 => assert DEFAULT_DELAY = 3 ns and tpd = 3 ns severity failure;
      when 3 => assert DEFAULT_DELAY = 1 ns and tpd = 5 ns severity failure;
      when others => assert false severity failure;
    end case;
    wait;
  end process;
end architecture;

entity t_vhdl_compgendflt is
end entity;

architecture t of t_vhdl_compgendflt is
  component t_vhdl_compgendflt_sub is
    generic (DEFAULT_DELAY : time := 1 ns; tpd : time := DEFAULT_DELAY; id : integer);
  end component;
begin
  u1 : component t_vhdl_compgendflt_sub generic map (id => 1);
  u2 : component t_vhdl_compgendflt_sub generic map (DEFAULT_DELAY => 3 ns, id => 2);
  u3 : component t_vhdl_compgendflt_sub generic map (tpd => 5 ns, id => 3);
  process
  begin
    wait for 1 ns;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
