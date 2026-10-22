# Sharp68000Box360

Projeto experimental de emulador do **Sharp X68000 para Xbox 360 (RGH/JTAG)**, usando libXenon e compilação na nuvem pelo GitHub Actions.

> **Estado real:** a versão atual é um protótipo de inicialização e integração inicial. Ainda não é compatível com jogos comerciais nem implementa o hardware completo do X68000.

## O que existe nesta etapa

- Fluxo de compilação na nuvem com a imagem `free60/libxenon`.
- Geração e publicação de artefatos ELF quando a execução conclui com sucesso.
- Núcleo Motorola 68000 experimental com uma fatia limitada de instruções: NOP, RTS, MOVEQ, MOVE.B/W/L entre registradores de dados, alguns modos de MOVE com `(An)`, `(An)+`, `-(An)` e endereçamento absoluto `.W/.L`, MOVEA.W/L, LEA `(An)`, `d16(An)`, indexado breve `d8(An,Xn)`/`d8(PC,Xn)`, absoluto `.W/.L` e relativo ao PC, MOVE imediato para Dn, ADDQ/SUBQ Dn e An (word/long em An, sem alterar flags), com atualização de C/X e flags aritméticas testadas, CLR/TST Dn, BRA/Bcc, BSR, JMP absoluto e STOP. A cobertura é parcial; ciclos, flags e modos de endereçamento ainda precisam de auditoria.
- Barramento experimental com 1 MiB de RAM, janela de ROM de 256 KiB e contadores de acessos não mapeados/escritas ignoradas.
- Scheduler inicial com orçamento de ciclos por quadro e distribuição do resto da divisão para reduzir deriva entre quadros.
- Save state/load state versão 1: serialização determinística em big-endian, cabeçalho com versão/tamanho, CRC32 para detectar corrupção, snapshot dos registradores 68000, estado do scheduler, RAM e contadores do barramento; carrega em duas fases para rejeitar arquivos inválidos antes de modificar a máquina. O estado não inclui a imagem da ROM (exige ROM já carregada compatível) nem periféricos ainda não implementados.

- Testes host para barramento (proteção de ROM, preservação da ROM após carga inválida, inicialização nula segura e máscara de endereço de 24 bits), reset, opcode não suportado, flags de ADDQ/SUBQ em Dn (C/X/N/Z/V) e preservação de SR em ADDQ/SUBQ para An e resultados de 32 bits em operações `.W` de An, BSR/RTS, MOVE com acesso à memória e endereçamento absoluto curto/longo, LEA absoluto `.W/.L`, relativo ao PC, indireto/deslocamento e indexado breve, com preservação de flags e rejeição dos bits reservados (incluindo o campo de escala), e distribuição fracionária de ciclos do scheduler, inclusive quando a frequência da CPU é inferior à frequência de quadros.

## O que ainda falta

- Conjunto completo de instruções e exceções do Motorola 68000, incluindo auditoria sistemática de flags, ciclos, alinhamento e falhas de barramento.
- Mapa de memória real do X68000, MMIO e controladores.
- Carga de IPL/ROMs e mídia de jogos.
- Vídeo, áudio, teclado, armazenamento, temporização de dispositivos e interrupções.
- Validação de desempenho/compatibilidade no Xbox 360 real.
- Conversão e validação de `default.xex`.

## Compilar na nuvem

1. Abra [GitHub Actions](https://github.com/MeiKinosaki/Sharp68000Box360/actions).
2. Selecione **Xenon toolchain cloud build**.
3. Execute a branch `cloud/xenon-toolchain` ou aguarde a execução automática após um commit.
4. Abra a execução e baixe o artefato `sharp68000box360-xenon-cloud-build`.

A compilação bem-sucedida de um ELF valida apenas o bootstrap atual; não comprova compatibilidade com jogos.

## Branch de desenvolvimento

- `cloud/xenon-toolchain`
- [Pull request](https://github.com/MeiKinosaki/Sharp68000Box360/pull/1)

## Licença

Consulte o arquivo [LICENSE](LICENSE).
